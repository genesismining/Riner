#pragma once

#include <optional.hpp>

namespace miner {

    template<class T>
    using optional_ref = std::experimental::optional<T&>;

    template<class T>
    using optional_cref = std::experimental::optional<const T&>;
    
    namespace opt = std::experimental;

    template<class T>
    using optional = opt::optional<T>;

    constexpr opt::nullopt_t nullopt = opt::nullopt;

}
