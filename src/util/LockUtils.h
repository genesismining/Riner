
#pragma once

#include <atomic>
#include <mutex>
#include <shared_mutex>
#include <src/common/Assert.h>
#include <src/util/Copy.h>

namespace miner {

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


    template<class T, class Lock = std::unique_lock<typename T::mutex_type>>
    class Locked {
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

    public:

        Locked(Locked &&) noexcept = default;
        Locked &operator=(Locked &&) noexcept = default;

        ~Locked() {
            if (lock) {
                guarded.get().refCount.fetch_add(-1, std::memory_order_relaxed);
            }
        }

        inline operator bool() const {return lock;}

        inline std::conditional_t<std::is_const<T>::value, std::add_const_t<typename T::value_type>, typename T::value_type> &operator*() {
            MI_EXPECTS(lock);
            return guarded.get().t;
        }
        inline std::conditional_t<std::is_const<T>::value, std::add_const_t<typename T::value_type>, typename T::value_type> *operator->() {return &operator*();}

        inline const typename T::value_type *operator->() const {
            MI_EXPECTS(lock);
            return guarded.get().t;
        }
        inline const typename T::value_type &operator *() const {return  operator*();}
    };

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

        auto lock() const -> Locked<std::remove_reference_t<decltype(*this)>> {
            return {*this};
        }

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
        template<class U, class M> friend class UpgradeableLockGuarded;

    protected:

        ReadLocked(const UpgradeableLockGuarded<T, Mutex> &t)
                : Locked<const UpgradeableLockGuarded<T, Mutex>, std::shared_lock<Mutex>>(t) {
        }

    public:

        ReadLocked(ReadLocked &&) noexcept = default;
        ReadLocked &operator=(ReadLocked &&) noexcept = default;

        WriteLocked<T, Mutex> upgrade() {
            this->guarded.get().upgradeMut.lock();
            // move this->lock to new object
            auto mut = this->lock.release();
            mut->unlock_shared();
            this->guarded.get().refCount.fetch_add(-1, std::memory_order_relaxed);
            return {this->getMutableRef()}; // now we own a unique lock
        }
    };

    template<class T, class Mutex>
    class WriteLocked : public Locked<UpgradeableLockGuarded<T, Mutex>, std::unique_lock<Mutex>> {
        template<class U, class M> friend class ReadLocked;
        template<class U, class M> friend class UpgradeableLockGuarded;

    protected:

        WriteLocked(UpgradeableLockGuarded<T, Mutex> &t)
                : Locked<UpgradeableLockGuarded<T, Mutex>, std::unique_lock<Mutex>>(t) {
        }

    public:

        WriteLocked(WriteLocked &&) noexcept = default;
        WriteLocked &operator=(WriteLocked &&) noexcept = default;

        ~WriteLocked() {
            // check whether the object was moved
            if (this->lock) {
                this->lock.release()->unlock();
                this->guarded.get().upgradeMut.unlock();
            }
        }

        ReadLocked<T, Mutex> downgrade() {
            // move this->lock to new object
            auto mut = this->lock.release();
            mut->unlock();
            this->guarded.get().refCount.fetch_add(-1, std::memory_order_relaxed);
            auto ret = ReadLocked<T, Mutex>{this->getConstRef()}; // shared lock is owned now
            this->guarded.get().upgradeMut.unlock();
            return ret;
        }
    };


    template<class T, class SharedMutex>
    class UpgradeableLockGuarded {
        template<class U, class M> friend class Locked;
        template<class U, class M> friend class WriteLocked;
        template<class U, class M> friend class ReadLocked;

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