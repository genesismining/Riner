//
//

#include "StringUtils.h"

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

}