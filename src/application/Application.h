#pragma once

#include <iostream>
#include <src/common/Optional.h>
#include <src/common/StringSpan.h>
#include <src/algorithm/Algorithm.h>
#include <src/pool/Pool.h>
#include <src/common/WorkCommon.h>
#include <src/compute/ComputeModule.h>
#include <src/pool/PoolSwitcher.h>
#include <src/application/ApiServer.h>
#include <deque>
#include "Config.h"

namespace miner {

    class Application {

        Config config; //referenced by other subsystem, and thus must outlive them

        unique_ptr<ComputeModule> compute; //lazy init, depends on config

        //This below is implicitly assuming that the same Gpu cannot be used by 2 AlgoImpls simultaneoulsy since they share the AlgoSettings. If this ever changes this vector must be something else
        LockGuarded<std::deque<optional<Device>>> devicesInUse; //nullopt if device is not used by any algo, same indexing as ConfigModule::getAllDeviceIds()

        //this array is accessed with the AlgoEnum and may contain nullptr
        std::array<unique_ptr<PoolSwitcher>, kAlgoTypeCount> poolSwitchers;

        std::list<unique_ptr<Algorithm>> algorithms;

        unique_ptr<ApiServer> apiServer; //lazy init, uses devicesInUse

        void launchProfile(const Config &config, Config::Profile &prof);

    public:
        explicit Application(optional<std::string> configPath);

        void logLaunchInfo(const std::string &implName, std::vector<std::reference_wrapper<Device>> &assignedDevices) const;
    };

}