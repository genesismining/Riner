
#pragma once

#include <atomic>
#include <mutex>
#include <shared_mutex>
#include <src/common/Assert.h>
#include <src/util/Copy.h>

namespace riner {

    template<class T, class Mutex = std::mutex>
    class LockGuarded;

    template<class T, class SharedMutex = std::shared_timed_mutex>
    class SharedLockGuarded;

    template<class T, class SharedMutex = std::shared_timed_mutex>
    class UpgradeableLockGuarded;


    template<class T, class SharedMutex = std::shared_timed_mutex>
    class WriteLocked;

    template<class T, class SharedMutex = std::shared_timed_mutex>
    class ReadLocked;

    /**
     * @brief return type of LockGuarded<V>::lock() method
     * Locked allows atomic access to an instance of V by locking a mutex.
     * The lock is released upon the destruction of the Locked instance.
     * It's mainly used as the return type of LockGuarded<V>::lock() method.
     *
     * @tparam T the LockGuarded<V> that generated this instance via its lock() method
     * @tparam Lock the underlying mutex ownership wrapper used to provide the locking functionality
     */
    template<class T, class Lock = std::unique_lock<typename T::mutex_type>>
    class Locked {
        template<class U, class M> friend class Locked;
        template<class U, class M> friend class LockGuarded;
        template<class U, class M> friend class SharedLockGuarded;

    protected:
        Lock lock;
        std::reference_wrapper<T> guarded;

        std::remove_const_t<T> &getMutableRef() const {
            return const_cast<std::remove_const_t<T>&>(guarded.get());
        }

        std::add_const_t<T> &getConstRef() const {
            return guarded.get();
        }

        Locked(const std::reference_wrapper<T> t)
                : lock(t.get().mut)
                , guarded(t) {
            guarded.get().refCount.fetch_add(1, std::memory_order_relaxed);
        }

        template<class U, class L>
        Locked(Locked<U, L> &&other) : guarded(other.getMutableRef()) {
            other.lock.unlock();
            lock = Lock(other.guarded.get().mut);
        }

    public:

        typedef typename T::value_type value_t;

        Locked(Locked &&) noexcept = default;
        Locked &operator=(Locked &&) noexcept = default;

        ~Locked() {
            if (lock) {
                guarded.get().refCount.fetch_add(-1, std::memory_order_relaxed);
            }
        }

        inline operator bool() const {return lock;}

        inline std::conditional_t<std::is_const<T>::value, std::add_const_t<value_t>, value_t> &operator*() {
            RNR_EXPECTS(lock);
            return guarded.get().t;
        }
        inline std::conditional_t<std::is_const<T>::value, std::add_const_t<value_t>, value_t> *operator->() {
            return &operator*();
        }

        inline const value_t *operator->() const {
            RNR_EXPECTS(lock);
            return guarded.get().t;
        }
        inline const value_t &operator *() const {
            return  operator*();
        }
    };


    /**
     * @brief Wrapper around an instance t of T that is only exclusively and atomically accessible after acquiring a lock.
     * LockGuarded contains an instance t that is associated with a mutex. t is only accessible via the object returned by the lock() medhod,
     * which returns an object of type Locked<...>. This Locked<...> object acts as a RAII-style lock guard locking the mutex,
     * but also provides operator-> and operator* for accessing the instance t.
     * This was inspired by lock_guard and the mutex<T> type of the rust language.
     *
     * @tparam T type of the instance which is to be protected by the mutex
     * @tparam Mutex the underlying mutex implementation used to provide the locking functionality
     */

    template<class T, class Mutex>
    class LockGuarded {
        template<class U, class M> friend class Locked;

    protected:
        T t;
        mutable std::atomic_int refCount {0};
        mutable Mutex mut;

    public:
        using value_type = T;
        using mutex_type = Mutex;

        DELETE_COPY_AND_MOVE(LockGuarded); //delete move, because this->t is referenced by return value of lock()

        template<class ... Args>
        LockGuarded(Args &&...args)
                : t(std::forward<Args>(args) ...) {
        }

        /**
         *
         * @return Locked<...> that allows access to only the const interface of the wrapped T via operator* and ->
         */
        auto lock() const -> Locked<std::remove_reference_t<decltype(*this)>> {
            return {*this};
        }

        /**
         *
         * @return Locked<...> that allows access to the wrapped T via operator* and ->
         */
        auto lock() -> Locked<std::remove_reference_t<decltype(*this)>> {
            return {*this};
        }

        ~LockGuarded() {
            auto _refCount = refCount.load(std::memory_order_relaxed);
            CHECK_EQ(_refCount, 0) << "Dangling reference to LockGuarded object detected: refCount = " << _refCount
                                   << std::endl << el::base::debug::StackTrace();
        }
    };



    /**
     * @brief Wrapper around an instance t of T which provides either exclusive read/write or shared read only access to the instance t
     * The object returned by the lock() method acquires exclusive lock and t can be read and written through it.
     * The method readLock() aquires a shared lock and t can be accessed read only through the returned object.
     *
     * @tparam T type of the instance which is to be protected by a shared mutex
     * @tparam SharedMutex the underlying shared mutex implementation used to provide the locking functionality
     */
    template<class T, class SharedMutex>
    class SharedLockGuarded : public LockGuarded<T, SharedMutex> {

    public:

        DELETE_COPY_AND_MOVE(SharedLockGuarded); //delete move, because this->t is referenced by return value of lock()

        template<class ... Args>
        SharedLockGuarded(Args &&...args)
                : LockGuarded<T, SharedMutex>(std::forward<Args>(args) ...) {
        }

        Locked<const SharedLockGuarded<T, SharedMutex>, std::shared_lock<SharedMutex>> readLock() {
            return {*this};
        }

        Locked<const SharedLockGuarded<T, SharedMutex>, std::shared_lock<SharedMutex>> readLock() const {
            return {*this};
        }

    };


    /**
     * @brief Provides (shared) atomic, read only access to a wrapped instance of T
     *
     * @tparam T type of the instance which is to be protected by a shared mutex
     * @tparam SharedMutex the underlying shared mutex implementation used to provide the locking functionality
     */
    template<class T, class SharedMutex>
    class ReadLocked : public Locked<const UpgradeableLockGuarded<T, SharedMutex>, std::shared_lock<SharedMutex>> {
        template<class U, class M> friend class WriteLocked;
        template<class U, class M> friend class ImmediateLocked;
        template<class U, class M> friend class UpgradeableLockGuarded;

    protected:

        ReadLocked(const UpgradeableLockGuarded<T, SharedMutex> &t)
                : Locked<const UpgradeableLockGuarded<T, SharedMutex>, std::shared_lock<SharedMutex>>(t) {
        }

        ReadLocked(Locked<UpgradeableLockGuarded<T, SharedMutex>, std::unique_lock<SharedMutex>> &&other)
                : Locked<const UpgradeableLockGuarded<T, SharedMutex>, std::shared_lock<SharedMutex>>(std::move(other)) {
        }

        ReadLocked(Locked<const UpgradeableLockGuarded<T, SharedMutex>, std::shared_lock<SharedMutex>> &&other)
                : Locked<const UpgradeableLockGuarded<T, SharedMutex>, std::shared_lock<SharedMutex>>(std::move(other)) {
        }

    public:

        ReadLocked(ReadLocked &&) noexcept = default;
        ReadLocked &operator=(ReadLocked &&) noexcept = default;

        /**
         * @brief moves object
         * Dangerous: can cause a deadlock when used together with UpgradeableLockGuarded::lock()
         *
         * @return WriteLocked<T, SharedMutex> through which the instance t can be accessed read only
         */
        WriteLocked<T, SharedMutex> upgrade() {
            this->guarded.get().upgradeMut.lock();
            return std::move(*this);
        }
    };

    /**
     * @brief Provides (shared) atomic, read only access to a wrapped instance of T, which can be upgraded atomically to (exclusive) read/write access
     * Please note that instances of this class should be downgraded to read locks or upgraded to write locks as soon as possible
     * because additional read locks cannot be aquired during the lifetime of this object.
     * After requesting a write lock no other locks can be acquired, which means that the write lock will be
     * acquired as soon as possible
     *
     * @tparam T type of the instance which is to be protected by a shared mutex
     * @tparam SharedMutex the underlying shared mutex implementation used to provide the locking functionality
     */
    template<class T, class SharedMutex>
    class ImmediateLocked : public Locked<const UpgradeableLockGuarded<T, SharedMutex>, std::shared_lock<SharedMutex>> {
        template<class U, class M> friend class ReadLocked;
        template<class U, class M> friend class WriteLocked;
        template<class U, class M> friend class UpgradeableLockGuarded;

    protected:

        ImmediateLocked(const UpgradeableLockGuarded<T, SharedMutex> &t)
                : Locked<const UpgradeableLockGuarded<T, SharedMutex>, std::shared_lock<SharedMutex>>(t) {
        }

        ImmediateLocked(Locked<UpgradeableLockGuarded<T, SharedMutex>, std::unique_lock<SharedMutex>> &&other)
                : Locked<const UpgradeableLockGuarded<T, SharedMutex>, std::shared_lock<SharedMutex>>(std::move(other)) {
        }

    public:

        ImmediateLocked(ImmediateLocked &&) noexcept = default;
        ImmediateLocked &operator=(ImmediateLocked &&) noexcept = default;

        ~ImmediateLocked() {
            // check whether the object was moved
            if (this->lock) {
                this->guarded.get().refCount.fetch_add(-1, std::memory_order_relaxed);
                this->lock.unlock();
                this->guarded.get().upgradeMut.unlock();
            }
        }

        /**
         * @brief moves object and atomically aquires unique access to the instance t
         *
         * @return WriteLocked<T, SharedMutex> through which the instance t can be read or written
         */
        WriteLocked<T, SharedMutex> upgrade() {
            return std::move(*this);
        }

        /**
         * @brief moves object and acquires shared read only access to the instance t
         * After calling the method other threads can get shared access to t again.
         *
         * @return ReadLocked<T, SharedMutex> through which the instance t can be read only
         */
        ReadLocked<T, SharedMutex> downgrade() {
            ImmediateLocked ret = std::move(*this);
            ret.guarded.get().upgradeMut.unlock();
            return std::move(ret);
        }
    };

    /**
     * @brief Provides (exclusive) atomic, read/write access to a wrapped instance of T, which can be downgraded atomically to (shared) read only access
     *
     * @tparam T type of the instance which is to be protected by a shared mutex
     * @tparam SharedMutex the underlying shared mutex implementation used to provide the locking functionality
     */
    template<class T, class SharedMutex>
    class WriteLocked : public Locked<UpgradeableLockGuarded<T, SharedMutex>, std::unique_lock<SharedMutex>> {
        template<class U, class M> friend class ReadLocked;
        template<class U, class M> friend class ImmediateLocked;
        template<class U, class M> friend class UpgradeableLockGuarded;

    protected:

        WriteLocked(UpgradeableLockGuarded<T, SharedMutex> &t)
                : Locked<UpgradeableLockGuarded<T, SharedMutex>, std::unique_lock<SharedMutex>>(t) {
        }

        WriteLocked(Locked<const UpgradeableLockGuarded<T, SharedMutex>, std::shared_lock<SharedMutex>> &&other)
                : Locked<UpgradeableLockGuarded<T, SharedMutex>, std::unique_lock<SharedMutex>>(std::move(other)) {
        }

    public:

        WriteLocked(WriteLocked &&) noexcept = default;
        WriteLocked &operator=(WriteLocked &&) noexcept = default;

        ~WriteLocked() {
            // check whether the object was moved
            if (this->lock) {
                this->guarded.get().refCount.fetch_add(-1, std::memory_order_relaxed);
                this->lock.unlock();
                this->guarded.get().upgradeMut.unlock();
            }
        }

        /**
         * @brief moves object and downgrades exclusive read/write access to shared read only access
         *
         * @return ReadLocked<T, SharedMutex> through which the instance t can be accessed read only
         */
        ReadLocked<T, SharedMutex> downgrade() {
            ReadLocked<T, SharedMutex> ret = std::move(*this); // shared lock is owned now
            ret.guarded.get().upgradeMut.unlock();
            return ret;
        }
    };

    /**
     * @brief Wrapper around an instance t of T which provides either exclusive read/write or shared read only access to the instance t
     * A similar class is SharedLockGuarded, but this class allows atomic upgrades or downgrades of locks, e.g.
     * from shared read only access to exclusive read/write access. It has more overhead than the former class because
     * it uses an additional mutex. Furthermore there are more guarantees that queued exclusive locks will be acquired
     * before queued shared locks.
     *
     * @tparam T type of the instance which is to be protected by a shared mutex
     * @tparam SharedMutex the underlying shared mutex implementation used to provide the locking functionality
     */
    template<class T, class SharedMutex>
    class UpgradeableLockGuarded {
        template<class U, class M> friend class Locked;
        template<class U, class M> friend class WriteLocked;
        template<class U, class M> friend class ReadLocked;
        template<class U, class M> friend class ImmediateLocked;

    protected:
        T t;
        mutable std::atomic_int refCount {0};
        mutable std::mutex upgradeMut;
        mutable SharedMutex mut;

    public:
        using value_type = T;
        using mutex_type = SharedMutex;

        DELETE_COPY_AND_MOVE(UpgradeableLockGuarded);

        template<class ... Args>
        UpgradeableLockGuarded(Args &&...args)
                : t(std::forward<Args>(args) ...) {
        }

        /**
         * @brief aquires exclusive write access to t, which can be downgraded to shared read only access later
         * For details: refer to WriteLocked
         *
         * @return WriteLocked<T, SharedMutex> through which the instance t can be accessed
         */
        WriteLocked<T, SharedMutex> lock() const {
            upgradeMut.lock();
            return {*this};
        }

        WriteLocked<T, SharedMutex> lock() {
            upgradeMut.lock();
            return {*this};
        }

        /**
         * @brief aquires shared read only access to t, which can be upgraded to exlusive read/write access later
         * For details: refer to ImmediateLocked
         *
         * @return ImmediateLocked<T, SharedMutex> through which the instance t can be accessed
         */
        ImmediateLocked<T, SharedMutex> immediateLock() const {
            upgradeMut.lock();
            return {*this};
        }

        ImmediateLocked<T, SharedMutex> immediateLock() {
            upgradeMut.lock();
            return {*this};
        }

        /**
         * @brief aquires shared read only access to t
         *
         * @return ReadLocked<T, SharedMutex> through which the instance t can be accessed
         */
        ReadLocked<T, SharedMutex> readLock() const {
            std::lock_guard<std::mutex> lk(upgradeMut);
            return {*this};
        }

        ReadLocked<T, SharedMutex> readLock() {
            std::lock_guard<std::mutex> lk(upgradeMut);
            return {*this};
        }

        ~UpgradeableLockGuarded() {
            auto _refCount = refCount.load(std::memory_order_relaxed);
            CHECK_EQ(_refCount, 0) << "Dangling reference to UpgradeableLockGuarded object detected: refCount = "
                                   << _refCount << std::endl << el::base::debug::StackTrace();
        }

    };

}
