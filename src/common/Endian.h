

#pragma once

#include "portable_endian.h"

#include <src/common/Span.h>

namespace riner {

    /**
     * os specific constants for figuring out/dealing with endianness
     */
    namespace endian {
        using endian_t = uint32_t;
#ifdef _WIN32
        constexpr endian_t little = 1234;
        constexpr endian_t big    = 4321;

        constexpr endian_t host = little;
#else
        constexpr endian_t little = __ORDER_LITTLE_ENDIAN__;
        constexpr endian_t big    = __ORDER_BIG_ENDIAN__;

        constexpr endian_t host = __BYTE_ORDER__;
#endif
        /**
         * whether endianness of the system you're compiling on is little/big respectively
         */
        constexpr bool is_little  = host == little;
        constexpr bool is_big     = host == big;
    }

    /**
     * "less-than" compare two large little endian unsigned integer numbers
     * param a span to first little endian number
     * param b span to second little endian number
     * return whether a < b if a and b are interpreted as large unsigned integers
     */
    static inline bool lessThanLittleEndian(cByteSpan<> a, cByteSpan<> b) {
        return std::lexicographical_compare(
                a.rbegin(), a.rend(),
                b.rbegin(), b.rend());
    }

}
