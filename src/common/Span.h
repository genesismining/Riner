#pragma once

#include <gsl/span>

namespace miner {
    
    template<class T, std::ptrdiff_t Extent = gsl::dynamic_extent>
    using span = gsl::span<T, Extent>;

    template<std::ptrdiff_t Extent = gsl::dynamic_extent>
    using ByteSpan = span<uint8_t, Extent>;

    template<std::ptrdiff_t Extent = gsl::dynamic_extent>
    using cByteSpan = span<const uint8_t, Extent>;
}