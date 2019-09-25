
#pragma once

#include <variant.hpp>

namespace miner {

    namespace var = mpark;

    template<class T, class Variant, class Func>
    bool visit(Variant &var, Func &&func) {
        if (auto ptr = var::get_if<T>(&var)) {
            func(*ptr);
            return true;
        }
        return false;
    }

    template<class... T>
    using variant = var::variant<T...>;

}
