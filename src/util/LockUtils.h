
#pragma once

#include <mutex>

namespace miner {

    template<class T>
    class Locked {
        std::unique_lock<std::mutex> lock;
        std::reference_wrapper<T> t;

    public:
        Locked(std::mutex &mut, T &t)
                : lock(mut)
                , t(t) {
        }

        Locked(Locked &&) noexcept = default;
        Locked &operator=(Locked &&) noexcept = default;

        T *operator->() {return &t.get();}
        T &operator *() {return  t.get();}
        const T *operator->() const {return &t.get();}
        const T &operator *() const {return  t.get();}

        operator bool() const {return true;}
    };

    template<class T>
    class LockGuarded {
        T t;
        std::mutex mut;

    public:
        using value_type = T;

        template<class ... Args>
        LockGuarded(Args &&...args)
                : t(std::forward<Args>(args) ...) {
        }

        Locked<T> lock() {
            return {mut, t};
        }

        Locked<const T> lock() const {
            return {mut, t};
        }

    };

}