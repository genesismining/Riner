
#pragma once

#include <variant.hpp>
#include <utility>

namespace miner {

    namespace mp = mpark;

    template<class T, class Variant, class Func>
    bool visit(Variant &var, Func &&func) {
        if (auto ptr = mp::get_if<T>(&var)) {
            func(*ptr);
            return true;
        }
        return false;
    }

}
