#pragma once

#include <optional.hpp>

namespace riner {
    
    /**
     * optional_ref is a relic from when we had an optional library that didn't support reference types.
     * feel free to refactor it out.
     */
    template<class T>
    using optional_ref = std::experimental::optional<T&>;

    /**
     * optional_cref (cref means const ref) is a relic from when we had an optional library that didn't support reference types.
     * feel free to refactor it out.
     */
    template<class T>
    using optional_cref = std::experimental::optional<const T&>;
    
    namespace opt = std::experimental;

    /**
     * std::optional alternative for < c++17
     */
    template<class T>
    using optional = opt::optional<T>;

    /**
     * std::nullopt alternative for < c++17
     */
    constexpr opt::nullopt_t nullopt = opt::nullopt;

}
