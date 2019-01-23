
#pragma once

#include <string>
#include <src/common/Optional.h>

namespace miner {

    struct ParsePoolAddressResult {
        std::string host;
        std::string port;
    };

    ParsePoolAddressResult parsePoolAddress(const char *str);

}
