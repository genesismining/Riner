
#include "HexString.h"
#include <string>
#include <algorithm> //std::min
#include <sstream>
#include <iomanip>

namespace miner {

    HexString::HexString(cstring_span inStr) {
        parseSuccess = parse(inStr);
    }

    HexString::HexString(cByteSpan<> src)
            : bytes(src.begin(), src.end())
            , parseSuccess(true) {
    }

    bool HexString::parse(cstring_span in) {

        //cut away "0x" if needed
        if (in.size() > 2 && in.subspan(0, 2) == "0x")
            in = in.subspan(2);

        const char *hexChars = "0123456789ABCDEF";
        auto start = (in.size()+1) % 2; //start at 0 if size is odd, 1 if even

        std::vector<uint8_t> result;
        result.reserve((size_t(in.size()) + 1) / 2);

        for (auto i = start; i < in.size(); i += 2) {

            char pair[2] = {//the two chars that make a byte
                    (i == 0) ? '0' : in[i-1], //add leading zero if needed
                    in[i],
            };

            //convert chars to their numeric value
            for (auto &c : pair) {
                if (auto *pos = strchr(hexChars, std::toupper(c)))
                    c = gsl::narrow_cast<uint8_t>(pos - hexChars);
                else
                    return false; //failure: contains at least one non-hex char
            }

            auto value = pair[0] * 16 + pair[1];

            result.push_back(gsl::narrow_cast<uint8_t>(value));
        }

        bytes = std::move(result);
        return true;
    }

    size_t HexString::sizeBytes() const {
        return bytes.size();
    }

    void HexString::reverseByteOrder() {
        std::reverse(begin(bytes), end(bytes));
    }

    HexString::operator bool() const {
        return parseSuccess;
    }

    std::string HexString::toString() const {
        std::stringstream stream;
        stream << std::hex << std::setfill('0');

        for(auto byte : bytes) {
            stream << std::setw(2) << static_cast<int>(byte);
        }

        return stream.str();
    }

    size_t HexString::getBytes(ByteSpan<> fullDst) const {
        auto fullSrc = cByteSpan<>{bytes};

        auto length = std::min(fullDst.size(), fullSrc.size());

        auto src = fullSrc.subspan(fullSrc.size() - length, length);
        auto dst = fullDst.subspan(fullDst.size() - length, length);

        //span to be filled with zeroes
        auto fill = fullDst.subspan(0, fullDst.size() - length);

        std::copy(src.begin(), src.end(), dst.begin());
        std::fill(fill.begin(), fill.end(), 0);

        return static_cast<size_t>(length);
    }

    /*
    // <delete this>
    void foobartestHex(cstring_span str, bool flipEndian) {

        if (HexString hex {str}) {

            if (flipEndian) {
                hex.reverseByteOrder();
            }

            auto bytes = hex.getBytes<8>();

            printf("\n");
            for (auto &byte : bytes) {
                if (byte != 0)
                    printf("%02X", byte);
                else
                    printf(". ");
            }
            printf("\n");

            auto hstr = hex.toString();
            printf("to string: %s\n", hstr.c_str());
        }
        else {
            printf("parsing this hexstring failed\n");
        }
    }

    int testfoobar() {

        foobartestHex("0x00000C0FFEEBAADF00D", false);
        foobartestHex("0x00000C0FFEEBAADF00D", true);
        foobartestHex("0xBAADF00D", false);
        foobartestHex("0xBAADF00D", true);
        exit(1);
    }
    int testt = testfoobar();
    // </delete this>
    */
}