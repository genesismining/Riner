
#pragma once

#include <type_safe/variant.hpp>

namespace miner {

    namespace ts = type_safe;

    template<class ... Ts>
    using variant = type_safe::variant<Ts...>;

    //TODO: figure out how to get this visit functionality with the methods the library provides. Otherwise switch to another variant lib
    template<class T, class Variant, class Func>
    bool visit(Variant &var, Func &&func) {
        bool matchingType = var.template has_value<T>({});
        if (matchingType) {
            using RefType = std::conditional_t<std::is_const<Variant>::value, const T &, T &>;

            RefType val = var.template value<T>({});
            func(val);
        }
        return matchingType;
    }

}
