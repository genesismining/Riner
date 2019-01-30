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

        Config config; //referenced by other subsystem, and thus must outlive them

        AlgoFactory algoFactory;
        PoolFactory poolFactory;

        unique_ptr<ComputeModule> compute; //lazy init, depends on config

        //this array is accessed with the AlgoEnum and may contain nullptr
        std::array<unique_ptr<PoolSwitcher>, kAlgoTypeCount> poolSwitchers;

        std::list<unique_ptr<AlgoBase>> algorithms;

        void launchProfile(const Config &config, Config::Profile &prof);

    public:
        explicit Application(optional<std::string> configPath);
    };

}