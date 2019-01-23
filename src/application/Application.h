#pragma once

#include <iostream>
#include <src/common/Optional.h>
#include <src/common/StringSpan.h>
#include <src/algorithm/AlgoFactory.h>
#include <src/pool/PoolFactory.h>
#include <src/common/WorkCommon.h>
#include <src/compute/ComputeModule.h>
#include <src/pool/PoolSwitcher.h>
#include "Config.h"

namespace miner {

    class Application {

        AlgoFactory algoFactory;
        PoolFactory poolFactory;

        ComputeModule compute;

        //this array is accessed with the AlgoEnum and may contain nullptr
        std::array<unique_ptr<PoolSwitcher>, kAlgoTypeCount> poolSwitchers;

        std::list<unique_ptr<AlgoBase>> algorithms;

        void launchProfile(const Config &config, Config::Profile &prof);

    public:
        explicit Application(optional<std::string> configPath);
    };

}