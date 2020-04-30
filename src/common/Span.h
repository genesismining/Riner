#pragma once

#include <gsl/span>

/**
* This header is mainly for shorthands when dealing with `gsl::span`s
* see the `gsl` or c++ core guidelines for more info on what a `gsl::span` is/does.
*/

namespace riner {
    
    template<class T, std::ptrdiff_t Extent = gsl::dynamic_extent>
    using span = gsl::span<T, Extent>;

    template<std::ptrdiff_t Extent = gsl::dynamic_extent>
    using ByteSpan = span<uint8_t, Extent>;

    template<std::ptrdiff_t Extent = gsl::dynamic_extent>
    using cByteSpan = span<const uint8_t, Extent>;
}
