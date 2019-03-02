
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

        if (configPath) {
            config = configUtils::loadConfig(configPath.value());
        } else {
            LOG(ERROR) << "no config path command line argument (--config /path/to/config.json)";
            return;
        }

        if (!config) {
            LOG(ERROR) << "no valid config available";
            return;
        }

        auto maybeProf = config.getStartProfile();
        if (!maybeProf) {
            LOG(WARNING) << "no start profile configured";
            return;
        }
        auto &prof = maybeProf.value();

        compute = std::make_unique<ComputeModule>(config);

        launchProfile(config, prof);
    }

    void Application::launchProfile(const Config &config, Config::Profile &prof) {
        using namespace configUtils;

        //clear old state
        algorithms.clear(); //clear algos before pools!

        for (auto &ptr : poolSwitchers)
            ptr.reset();

        //launch profile
        auto &allIds = compute->getAllDeviceIds();

        //find all Algo Implementations that need to be launched
        auto allRequiredImplNames = getUniqueAlgoImplNamesForProfile(prof, allIds);

        LOG(INFO) << "starting profile '" << prof.name << "'";

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

                auto poolImplName = poolFactory.getImplNameForAlgoTypeAndProtocol(algoType, p.protocol);
                auto pool = poolFactory.makePool(args, algoType, p.protocol);

                if (!pool) {
                    LOG(ERROR) << "no pool implementation available for algo type "
                    << stringFromAlgoEnum(algoType) << " in combination with protocol type "
                    << stringFromProtoEnum(p.protocol);
                    continue;
                }

                LOG(INFO) << "launching pool '" << poolImplName << "' to connect to " << p.host << " on port " << p.port;

                poolSwitcher->push(std::move(pool));
            }

            if (poolSwitcher->poolCount() == 0) {
                LOG(ERROR) << "no pools available for algoType of " << implName << ". Cannot launch algorithm";
            }

            poolSwitchers[algoType] = std::move(poolSwitcher);

            MI_ENSURES(poolSwitchers[algoType]);

            std::vector<DeviceAlgoInfo> deviceInfos =
                    getAllDeviceAlgoInfosForAlgoImplName(implName, config, prof, compute->getAllDeviceIds());

            std::string logText = "launching algorithm '" + implName + "' with devices: ";
            for (auto &devInfo : deviceInfos) {
                logText += "#" + std::to_string(devInfo.deviceIndex) + " " + gsl::to_string(devInfo.id.getName());
                if (&devInfo != &deviceInfos.back())
                    logText += ", ";
            }
            LOG(INFO) << logText;

            AlgoConstructionArgs args {
                *compute,
                deviceInfos,
                *poolSwitchers[algoType]
            };

            auto algo = algoFactory.makeAlgo(args, implName);

            algorithms.emplace_back(std::move(algo));
        }
    }

}









