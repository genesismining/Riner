

#pragma once

#include <array>
#include <src/common/Span.h>

namespace miner {

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
        constexpr bool is_little  = host == little;
        constexpr bool is_big     = host == big;
    }

    template<class T>
    std::array<uint8_t, sizeof(T)> conditionalSwapBytesArray(const T &src, bool cond) {
        std::array<uint8_t, sizeof(T)> array;
        memcpy(array.data(), &src, sizeof(T));
        if (cond) {
            std::reverse(array.begin(), array.end());
        }
        return array;
    }

    template<class T>
    typename std::enable_if<std::is_integral<T>::value, std::array<uint8_t, sizeof(T)>>::type
    toBytesWithEndian(T src, endian::endian_t endian) {
        return conditionalSwapBytesArray(src, endian != endian::host);
    }

    //htobe for arrays
    template<class T>
    typename std::enable_if<std::is_integral<T>::value, std::array<uint8_t, sizeof(T)>>::type
    toBytesWithBigEndian(T src) {
        return toBytesWithEndian(src, endian::big);
    }

    //htole for arrays
    template<class T>
    typename std::enable_if<std::is_integral<T>::value, std::array<uint8_t, sizeof(T)>>::type
    toBytesWithLittleEndian(T src) {
        return toBytesWithEndian(src, endian::little);
    }

    static inline bool lessThanLittleEndian(cByteSpan<> a, cByteSpan<> b) {
        return std::lexicographical_compare(
                a.rbegin(), a.rend(),
                b.rbegin(), b.rend());
    }

}