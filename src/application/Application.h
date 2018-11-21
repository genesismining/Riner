#pragma once

#include <iostream>
#include <src/common/Optional.h>
#include <src/common/StringSpan.h>

namespace miner {

    class Application {

        void parseConfig(cstring_span configPath);

    public:
        Application(optional<std::string> configPath);
    };

}