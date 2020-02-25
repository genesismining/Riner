//
//

#include "StringUtils.h"
#include <algorithm>
#include <type_traits>

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
                       [] (char c) { return static_cast<char>(std::tolower(c)); });
        return str;
    }

}