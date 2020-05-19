
#pragma once

#include <src/util/Logging.h>
#include <src/common/Optional.h>
#include <src/common/Span.h>
#include <src/common/JsonForward.h>
#include <string>

namespace riner {

    /**
     * class for extracting bytes from a hex-string
     * and converting bytes to a hex-string
     * common usage:
     * ``
     * if (HexString h {"0xC0FFEE"}) //also supports missing 0x
     *     auto bytes = h.getBytes<32>();
     * else
     *     std::cout << "parsing failed"`
     * ```
     */
    class HexString {

        std::vector<uint8_t> bytes;

        bool parse(const std::string &in);
        bool parseSuccess = false;
    public:
        
        /**
         * convenience ctor to create a HexString from a nl::json object that is a string
         */
        explicit HexString(const nl::json &jsonInStr);
        
        /**
         * parses the given std::string  and creates a HexString object from it
         * tolerates "0x" at the beginning, but doesn't require it
         */
        explicit HexString(const std::string &inStr);
        
        /**
         * initializes the HexString with bytes from a `cByteSpan`
         */
        explicit HexString(cByteSpan<> src);
        
        /**
         * create HexString from integral types like uint64_t values
         */
        template<class T>
        HexString(T t, typename std::enable_if<std::is_integral<T>::value>::type* = 0) = delete;
        
        /**
         * create HexString from owned byte arrays
         */
        template <size_t N>
        explicit HexString(const std::array<uint8_t, N> &arr)
        : HexString(cByteSpan<>(arr)) {
        }

        /**
         * reverse the byte order of the HexString
         * (does not affect where leading zeroes are placed in getBytes)
         */
        HexString &swapByteOrder();

        /**
         * @return lowercase hex without "0x"
         */
        std::string str() const;

        /**
         * @return amount of bytes e.g. "0x001F" => 2
         */
        size_t sizeBytes() const;

        //copies data into dst with leading zeroes
        //if dst is too small, leading bytes will be cut
        
        /**
         * copies data into dst with leading zeroes.
         * if dst is too small, leading bytes will be cut.
         * @return the length of the bytes written to ByteSpan
         */
        size_t getBytes(ByteSpan<>) const;
        
        /**
         * convenience function that creates an array and calls getBytes
         */
        template<size_t N>
        std::array<uint8_t, N> getBytes() const {
            std::array<uint8_t, N> arr;
            getBytes({arr});
            return arr;
        }

        /**
         * @return false if parse failed
         */
        operator bool() const;
    };

}
