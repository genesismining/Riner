//
//

#include "StringUtils.h"
#include <algorithm>

namespace miner {

    ParsePoolAddressResult parsePoolAddress(const char *cstr) {
        ParsePoolAddressResult res;
        auto npos = std::string::npos;

        std::string str{cstr};

        auto colonPos = str.find(':');
        res.host = str.substr(0, colonPos);

        if (colonPos == npos && (colonPos != str.size() - 1)) {
            res.port = str.substr(colonPos+1);
        }
        return res;
    }

    std::string toLower(const std::string &inStr) {
        std::string str = inStr;
        std::transform(str.begin(), str.end(), str.begin(),
                [](unsigned char c){ return std::tolower(c); });
        return str;
    }

    bool startsWith(const std::string& string, const std::string& prefix) {
        return string.compare(0, prefix.length(), prefix) == 0;
    }


}
