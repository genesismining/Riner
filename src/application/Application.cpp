
#include <iostream>

#include "Application.h"
#include <src/util/FileUtils.h>
#include <src/application/Config.h>
#include <src/util/Logging.h>
#include <src/common/Optional.h>
#include <src/compute/ComputeModule.h>
#include <src/common/Json.h>
#include <src/pool/PoolEthash.h>
#include <src/algorithm/ethash/AlgoEthashCL.h>
#include <thread>
#include <src/algorithm/Algorithm.h>
#include <src/pool/PoolSwitcher.h>
#include <src/util/ConfigUtils.h>

namespace miner {

    Application::Application(optional<std::string> configPath) {
        using namespace std::chrono;

        Config config;

        if (configPath) {
            config = configUtils::loadConfig(configPath.value());
        } else {
            LOG(ERROR) << "no config path command line argument (--config /path/to/config.json)";
            return;
        }

        if (!config) {
            LOG(ERROR) << "no config available";
            return;
        }

        auto maybeProf = config.getStartProfile();
        if (!maybeProf) {
            LOG(WARNING) << "no start profile configured";
            return;
        }
        auto &prof = maybeProf.value();

        launchProfile(config, prof);

/*
#if 0
        std::string host = "eth-eu1.nanopool.org", port = "9999";
#else
        //std::string host = "localhost", port = "9998";
        std::string host = "192.168.30.39", port = "3001";
#endif

        ComputeModule compute;
        for (auto &id : compute.getAllDeviceIds()) {
            LOG(INFO) << "device id at: " << &id << " name: " << to_string(id.getName());
        };

        auto username = "user";
        auto password = "password";

        auto interval = seconds(1);
        auto timeUntilDead = seconds(5);
        PoolSwitcher poolSwitcher{interval, timeUntilDead};

        poolSwitcher.emplace<PoolEthashStratum>({
            host, port, username, password
        });

        poolSwitcher.emplace<PoolEthashStratum>({
            "eth-eu1.nanopool.org", "9999", username, password
        });

        //PoolEthashStratum poolEthashStratum({host, port, username, password});

        AlgoEthashCL algo({compute, compute.getAllDeviceIds(), poolSwitcher});

        for (size_t i = 0; i < 60 * 4; ++i) {
            std::this_thread::sleep_for(std::chrono::seconds(1));
            //LOG(INFO) << "...";
        }
        LOG(INFO) << "test period over, closing application";
        */
    }

    void Application::launchProfile(const Config &config, Config::Profile &prof) {
        using namespace configUtils;

        //clear old state
        algorithms.clear(); //clear algos before pools!

        for (auto &ptr : poolSwitchers)
            ptr.reset();

        //launch profile
        auto &allIds = compute.getAllDeviceIds();

        //find all Algo
        auto allRequiredImplNames = getUniqueAlgoImplNamesForProfile(prof, allIds);

        //for all algorithms that are required to be launched
        for (auto &implName : allRequiredImplNames) {

            auto algoType = algoFactory.getAlgoTypeForImplName(implName);
            auto configPools = getConfigPoolsForAlgoType(config, algoType);

            auto poolSwitcher = std::make_unique<PoolSwitcher>();


            for (auto &configPool : configPools) {
                auto &p = configPool.get();

                PoolConstructionArgs args {
                    p.host, p.port, p.username, p.password
                };

                auto pool = poolFactory.makePool(args, algoType, p.protocol);

                poolSwitcher->push(std::move(pool));
            }

            if (poolSwitcher->poolCount() == 0) {
                LOG(ERROR) << "no pools available for algoType of " << implName << ". Cannot launch algorithm";
            }

            poolSwitchers[algoType] = std::move(poolSwitcher);

            MI_ENSURES(poolSwitchers[algoType]);

            auto deviceInfos = getAllDeviceAlgoInfosForAlgoImplName(implName, config, prof, compute.getAllDeviceIds());

            AlgoConstructionArgs args {
                compute,
                deviceInfos,
                *poolSwitchers[algoType]
            };

            auto algo = algoFactory.makeAlgo(args, implName);

            algorithms.emplace_back(std::move(algo));
        }
    }


    /*
    void foo() {
        Config config("");

        auto &prof = config.getStartProfile().value();

        add "for all unique algoImplName in all profiles {"
        auto implName = prof.device_default.algoImplName; This is wrong, you start by iterating devs not by finding an algo

        AlgoFactory algoFac; //instantiates appropriate class for algo name string
        PoolFactory poolFac; //instantiates appropriate class for algoType and protocol

        auto algoType = factory.getAlgoTypeForName(implName);

        //create pools
        PoolSwitchr poolSwitcher;

        //for (auto &pool : config.getPoolsWithAlgoType(algoType));
        for (auto &cpool : config.getPools()) {

            if (cpool.type == algoType) {

                if (auto pool = poolFac.makePool(cpool.type, cpool.protocol))
                    poolSwitcher.push(std::move(pool));
            }
        }

        //gather devices
        ComputeModul compute;

        std::vector<DeviceInfo> devInfos;

        size_t i = 0;
        for (auto &devId : compute.getAllDeviceIds()) {
            //extract the following into a "getAlgoSettingsForDeviceId" function

            optional<Config::Profile::Mapping> mapping;

            //if there is an extra rule for device with index #i
            if (prof.devices_by_index.count(i)) {
                mapping = prof.devices_by_index.at(i);
            }
            else {
                mapping = prof.device_default;
            }

            auto &devp = config.getDeviceProfile(mapping.value().deviceProfileName).value();

            //are there settings for this algoImpl on this deviceProfile?
            if (auto configAlgoSettings = devp.getAlgoSettings(implName)) {
                devInfos.push_back({devId, configAlgoSettings.value()})
            }
            else {
                LOG(WARNING) << "deviceProfile '" << devp.name << "' does not offer settings for the requested algorithm '" << implName << "'";
            }

            ++i;
        }

        AlgoConstructionArgs args {
            compute, devInfos, poolSwitcher
        };

        unique_ptr<BaseAlgo> algo = factory.makeAlgo(implName, args);

    }
     */
}









