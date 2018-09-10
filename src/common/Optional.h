#pragma once

#include <type_safe/optional.hpp>
#include <type_safe/optional_ref.hpp>

namespace miner {

    constexpr type_safe::nullopt_t nullopt;
    
    template<class T>
    using optional = type_safe::optional<T>;
    
    template<class T>
    using optional_ref = type_safe::optional_ref<T>;
    
}
