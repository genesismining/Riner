
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


    template<class T, class Mutex = std::shared_timed_mutex>
    class WriteLocked;

    template<class T, class Mutex = std::shared_timed_mutex>
    class ReadLocked;

    /**
     * @brief return value of LockGuarded<V>'s lock() method
     * Locked allows access to a V instance while ensuring that a certain mutex is locked.
     * It's mainly used as the return type of Lockguarded<V>'s lock() method.
     * @tparam T the LockGuarded<V> that generated this instance via its lock() method
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
     * @brief Wrapper around an instance of T that is only accessible after acquiring a lock.
     * LockGuarded contains a T instance t that is associated with a mutex. t is only accessible via the lock()
     * method which returns an object of type Locked<...>. This Locked<...> object acts as a RAII-style lock guard, but also provides
     * operator-> and operator* for accessing the T instance.
     *
     * @tparam T type of the instance which is to be protected by the mutex
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



    template<class T, class Mutex>
    class ReadLocked : public Locked<const UpgradeableLockGuarded<T, Mutex>, std::shared_lock<Mutex>> {
        template<class U, class M> friend class WriteLocked;
        template<class U, class M> friend class ImmediateLocked;
        template<class U, class M> friend class UpgradeableLockGuarded;

    protected:

        ReadLocked(const UpgradeableLockGuarded<T, Mutex> &t)
                : Locked<const UpgradeableLockGuarded<T, Mutex>, std::shared_lock<Mutex>>(t) {
        }

        ReadLocked(Locked<UpgradeableLockGuarded<T, Mutex>, std::unique_lock<Mutex>> &&other)
                : Locked<const UpgradeableLockGuarded<T, Mutex>, std::shared_lock<Mutex>>(std::move(other)) {
        }

        ReadLocked(Locked<const UpgradeableLockGuarded<T, Mutex>, std::shared_lock<Mutex>> &&other)
                : Locked<const UpgradeableLockGuarded<T, Mutex>, std::shared_lock<Mutex>>(std::move(other)) {
        }

    public:

        ReadLocked(ReadLocked &&) noexcept = default;
        ReadLocked &operator=(ReadLocked &&) noexcept = default;

        /**
         * Dangerous: can cause a deadlock when used together with UpgradeableLockGuarded::lock()
         */
        WriteLocked<T, Mutex> upgrade() {
            this->guarded.get().upgradeMut.lock();
            return std::move(*this);
        }
    };

    template<class T, class Mutex>
    class ImmediateLocked : public Locked<const UpgradeableLockGuarded<T, Mutex>, std::shared_lock<Mutex>> {
        template<class U, class M> friend class ReadLocked;
        template<class U, class M> friend class WriteLocked;
        template<class U, class M> friend class UpgradeableLockGuarded;

    protected:

        ImmediateLocked(const UpgradeableLockGuarded<T, Mutex> &t)
                : Locked<const UpgradeableLockGuarded<T, Mutex>, std::shared_lock<Mutex>>(t) {
        }

        ImmediateLocked(Locked<UpgradeableLockGuarded<T, Mutex>, std::unique_lock<Mutex>> &&other)
                : Locked<const UpgradeableLockGuarded<T, Mutex>, std::shared_lock<Mutex>>(std::move(other)) {
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

        WriteLocked<T, Mutex> upgrade() {
            return std::move(*this);
        }

        ReadLocked<T, Mutex> downgrade() {
            ImmediateLocked ret = std::move(*this);
            ret.guarded.get().upgradeMut.unlock();
            return std::move(ret);
        }
    };


    template<class T, class Mutex>
    class WriteLocked : public Locked<UpgradeableLockGuarded<T, Mutex>, std::unique_lock<Mutex>> {
        template<class U, class M> friend class ReadLocked;
        template<class U, class M> friend class ImmediateLocked;
        template<class U, class M> friend class UpgradeableLockGuarded;

    protected:

        WriteLocked(UpgradeableLockGuarded<T, Mutex> &t)
                : Locked<UpgradeableLockGuarded<T, Mutex>, std::unique_lock<Mutex>>(t) {
        }

        WriteLocked(Locked<const UpgradeableLockGuarded<T, Mutex>, std::shared_lock<Mutex>> &&other)
                : Locked<UpgradeableLockGuarded<T, Mutex>, std::unique_lock<Mutex>>(std::move(other)) {
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

        ReadLocked<T, Mutex> downgrade() {
            ReadLocked<T, Mutex> ret = std::move(*this); // shared lock is owned now
            ret.guarded.get().upgradeMut.unlock();
            return ret;
        }
    };


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

        WriteLocked<T, SharedMutex> lock() const {
            upgradeMut.lock();
            return {*this};
        }

        WriteLocked<T, SharedMutex> lock() {
            upgradeMut.lock();
            return {*this};
        }

        ImmediateLocked<T, SharedMutex> immediateLock() const {
            upgradeMut.lock();
            return {*this};
        }

        ImmediateLocked<T, SharedMutex> immediateLock() {
            upgradeMut.lock();
            return {*this};
        }

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