#pragma once

#include <iostream>
#include <src/common/Optional.h>
#include <src/algorithm/Algorithm.h>
#include <src/pool/Pool.h>
#include <src/compute/ComputeModule.h>
#include <src/pool/PoolSwitcher.h>
#include <deque>
#include <map>
#include "src/config/Config.h"

namespace riner {
    class ApiServer;

    class Application {
    public: //everything public, makes introspection from tests simpler
        Config config; //referenced by other subsystem, and thus must outlive them

        ComputeModule compute; //lazy init, depends on config

        //This below is implicitly assuming that the same Gpu cannot be used by 2 AlgoImpls simultaneoulsy since they share the AlgoSettings. If this ever changes this vector must be something else
        SharedLockGuarded<std::deque<optional<Device>>> devicesInUse; //nullopt if device is not used by any algo, same indexing as ConfigModule::getAllDeviceIds()

        //algorithm name is the key of this map
        SharedLockGuarded<std::map<std::string, unique_ptr<PoolSwitcher>>> poolSwitchers;

        std::list<unique_ptr<Algorithm>> algorithms;

        unique_ptr<ApiServer> apiServer; //lazy init, stores reference to this `Application` instance

        void launchProfile(const Config &config, const proto::Config_Profile &prof);

        void logLaunchInfo(const std::string &implName, std::vector<std::reference_wrapper<Device>> &assignedDevices) const;

        explicit Application(Config config);
        ~Application();
    };

}