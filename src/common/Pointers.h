
#pragma once

#include <memory>

/**
 * This header is mainly for shorthands when dealing with c++11 smart pointers
 */

namespace riner {

    using std::unique_ptr;
    using std::shared_ptr;
    using std::weak_ptr;
    using std::make_unique;
    using std::make_shared;

    /**
     * cast a unique_ptr<U> to a unique_ptr<T> (like dynamic_cast)
     * @return the downcast version of uUnique
     */
    template<class T, class U>
    unique_ptr<T> static_unique_ptr_cast(unique_ptr<U> uUnique) {
        U *u = uUnique.release(); //drop ownership of *u
        T *t = u == nullptr ? nullptr : u->template downCast<T>(); //TODO: "downCast" does not belong here. this is a generic unique_ptr function that should not make assumptions about U
        return unique_ptr<T>(t); //make new owning unique_ptr for *t
    }

    /**
     * function that clearly indicates intent when creating a weak_ptr from a shared_ptr
     * @return a weak_ptr that points to the same object as `sharedPtr`
     */
    template<class T>
    std::weak_ptr<T> make_weak(const std::shared_ptr<T> &sharedPtr) {
        return sharedPtr;
    }

}

