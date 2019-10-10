//
//

#pragma once

#include <src/common/Optional.h>
#include <string>
#include <vector>

namespace miner {

    struct SslDesc {
        std::vector<std::string> certFiles; //if no cert file is provided, asio's set_default_verify_paths() is used
    };

}