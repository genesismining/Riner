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

    /**
     * class representing the application's state (excluding `ShutdownState`)
     */
    class Application {
    public: //everything public, makes introspection from tests simpler
        Config config; //referenced by other subsystem, and thus must outlive them

        ComputeModule compute; //lazy init, depends on config

        /**
         * Device objects currently referenced by a running AlgoImpl.
         * The Device objects in the deque have matching indices to ConfigModule::getAllDeviceIds(),
         * they contain nullopt if device is not used by any AlgoImpl.
         *
         * The declaration implicitly assumes that the same Device cannot be used by 2 AlgoImpls
         * simultaneoulsy since they share the AlgoSettings. If this ever changes this vector must be something else.
         */
        SharedLockGuarded<std::deque<optional<Device>>> devicesInUse;

        /**
         * map of PoolSwitcher (one `PoolSwitcher` per powType)
         * key: powType as specified in `Registry`
         * value: the running `PoolSwitcher` object (never nullptr. the unique_ptr is only put there for convenient std::move)
         */
        SharedLockGuarded<std::map<std::string, unique_ptr<PoolSwitcher>>> poolSwitchers;

        /**
         * running AlgoImpl instances. Elements are never nullptr.
         */
        std::list<unique_ptr<Algorithm>> algorithms;

        /**
         * Json RPC 2.0 monitoring Api server. stores reference to this `Application` instance and accesses its members
         */
        unique_ptr<ApiServer> apiServer;

        /**
         * launches a particular config profile (starts AlgoImpls, Pools, etc...)
         */
        void launchProfile(const Config &config, const proto::Config_Profile &prof);

        /**
         * logs information about the launch of a profile
         */
        void logLaunchInfo(const std::string &implName, std::vector<std::reference_wrapper<Device>> &assignedDevices) const;

        /**
         * start the Application with a `config`, will launch the start profile specified in the config!
         */
        explicit Application(Config config);
        ~Application();
    };

}
