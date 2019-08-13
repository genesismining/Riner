
#pragma once

#include <memory>

namespace miner {

    using std::unique_ptr;
    using std::shared_ptr;
    using std::weak_ptr;
    using std::make_unique;
    using std::make_shared;

    template<class T, class U>
    unique_ptr<T> static_unique_ptr_cast(unique_ptr<U> uUnique) {
        U *u = uUnique.release(); //drop ownership of *u
        T *t = u == nullptr ? nullptr : u->template downCast<T>();
        return unique_ptr<T>(t); //make new owning unique_ptr for *t
    }

    //convenience function for creating weak_ptr out of shared_ptr
    template<class T>
    std::weak_ptr<T> make_weak(const std::shared_ptr<T> &sharedPtr) {
        return sharedPtr;
    }

}

