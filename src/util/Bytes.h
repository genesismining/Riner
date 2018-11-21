
#pragma once

#include <array>
#include <src/common/Endian.h>

namespace miner {

    template<size_t N>
    using Bytes = std::array<uint8_t, N>;

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

}