
#pragma once

#include <src/util/Logging.h>
#include <src/common/StringSpan.h>
#include <src/common/Optional.h>
#include <src/common/Span.h>
#include <src/common/JsonForward.h>
#include <string>

namespace miner {

    //class for extracting bytes from hex-string
    //common usage:
    //
    //  if (HexString h {"0xC0FFEE"}) //also supports missing 0x
    //      auto bytes = h.getBytes<32>();
    //  else
    //      std::cout << "parsing failed"
    //

    class HexString {

        std::vector<uint8_t> bytes;

        bool parse(cstring_span inStr);
        bool parseSuccess = false;
    public:
        explicit HexString(const nl::json &jsonInStr);
        explicit HexString(const std::string &inStr);
        explicit HexString(cstring_span inStr);
        explicit HexString(cByteSpan<> src);

        template <size_t N>
        explicit HexString(const std::array<uint8_t, N> &arr)
        : HexString(cByteSpan<>(arr)) {
        }

        HexString &flipByteOrder(); //does not affect where leading zeroes are placed in getBytes

        std::string toString() const; //returns lowercase hex without "0x"

        //returns amount of bytes e.g. "0x001F" => 2
        size_t sizeBytes() const;

        //copies data into dst with leading zeroes
        //if dst is too small, leading bytes will be cut
        size_t getBytes(ByteSpan<> dst) const;

        //convenience function that creates an array and calls getBytes
        template<size_t N>
        std::array<uint8_t, N> getBytes() const {
            std::array<uint8_t, N> arr;
            getBytes({arr});
            return arr;
        }

        operator bool() const; //false if parse failed
        operator std::string() const;
    };

}