
#pragma once

#include <array>
#include <src/common/Endian.h>
#include <cstring>

namespace riner {

    /**
     * convenience template for arrays of a fixed number of bytes
     * @tparam N number of bytes
     */
    template<size_t N>
    using Bytes = std::array<uint8_t, N>;

    /**
     * takes a `Bytes<N>` and returns a copy of it. If the provided condition `cond` is `true` the byte order of that copy is reversed before it gets returned.
     * @tparam T a `Bytes<N>` type or similar
     * @param src the `Bytes<N>` object to be copied
     * @param cond if `true`, the byte order is reversed before returning
     * @return `Bytes<N>` object with either identical or reversed byte order of `src` decided by `cond`
     */
    template<class T>
    std::array<uint8_t, sizeof(T)> conditionalSwapBytesArray(const T &src, bool cond) {
        std::array<uint8_t, sizeof(T)> array;
        memcpy(array.data(), &src, sizeof(T));
        if (cond) {
            std::reverse(array.begin(), array.end());
        }
        return array;
    }

    /**
     * takes an integral type `T` (e.g. `int`...) and creates a `Bytes<sizeof(T)>` object out of it with a chosen endianness.
     * @param src the integral value to be converted
     * @param endian the endianness that the resulting `Bytes<N>` object should have
     * @return a `Bytes<sizeof(T)>` object containing the numerical value of `src` with the chosen endianness
     */
    template<class T>
    typename std::enable_if<std::is_integral<T>::value, Bytes<sizeof(T)>>::type
    toBytesWithEndian(T src, endian::endian_t endian) {
        return conditionalSwapBytesArray(src, endian != endian::host);
    }

    /**
     * Shorthand for `toBytesWithEndian()` with a big endian result (basically htobe for arrays).
     */
    template<class T>
    typename std::enable_if<std::is_integral<T>::value, Bytes<sizeof(T)>>::type
    toBytesWithBigEndian(T src) {
        return toBytesWithEndian(src, endian::big);
    }

    /**
     * Shorthand for `toBytesWithEndian()` with a little endian result (basically htole for arrays).
     */
    template<class T>
    typename std::enable_if<std::is_integral<T>::value, Bytes<sizeof(T)>>::type
    toBytesWithLittleEndian(T src) {
        return toBytesWithEndian(src, endian::little);
    }

}
