
#pragma once

#include <memory>

namespace miner {

    template<class T, class DeleterT = std::default_delete<T>>
    using unique_ptr = std::unique_ptr<T, DeleterT>;

    template<class T, class U>
    unique_ptr<T> static_unique_ptr_cast(unique_ptr<U> uUnique) {
        U *u = uUnique.release(); //drop ownership of *u
        T *t = static_cast<T *>(u);
        return unique_ptr<T>(t); //make new owning unique_ptr for *t
    }

    //convenience function for creating weak_ptr out of shared_ptr
    template<class T>
    std::weak_ptr<T> make_weak(std::shared_ptr<T> &sharedPtr) {
        return sharedPtr;
    }

}

