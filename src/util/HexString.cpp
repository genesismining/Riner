
#include "HexString.h"
#include <string>
#include <algorithm> //std::min
#include <sstream>
#include <iomanip>
#include <src/common/Json.h>

namespace miner {

    HexString::HexString(cstring_span inStr) {
        parseSuccess = parse(inStr);
    }

    HexString::HexString(cByteSpan<> src)
            : bytes(src.begin(), src.end())
            , parseSuccess(true) {

    }

    HexString::HexString(uint64_t src)
    : bytes(sizeof(src))
    , parseSuccess(true) {
        memcpy(bytes.data(), &src, sizeof(src));
    }

    HexString::HexString(const nl::json &json) {
        auto inStr = json.get<std::string>();
        parseSuccess = parse(inStr);
    }

    HexString::HexString(const std::string &inStr) {
        parseSuccess = parse(inStr);
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

    HexString &HexString::flipByteOrder() {
        std::reverse(begin(bytes), end(bytes));
        return *this;
    }

    HexString::operator bool() const {
        return parseSuccess;
    }

    std::string HexString::str() const {
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

}