
#pragma once

#include <string>
#include <src/common/Optional.h>

namespace miner {

    struct ParsePoolAddressResult {
        std::string host;
        std::string port;
    };

    ParsePoolAddressResult parsePoolAddress(const char *str);

    //returns lowercase version of inStr
    std::string toLower(const std::string &inStr);

}
