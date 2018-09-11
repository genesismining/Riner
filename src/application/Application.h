#pragma once

#include <iostream>
#include <src/common/Optional.h>
#include <src/common/StringSpan.h>

namespace miner {

    class Application {
    public:
        Application(optional<std::string> configPath);
    };

}