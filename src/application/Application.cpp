
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

        compute = std::make_unique<ComputeModule>(config);

        //init devicesInUse
        devicesInUse.lock()->resize(compute->getAllDeviceIds().size());

        auto port = config.getGlobalSettings().api_port;
        apiServer = make_unique<ApiServer>(port, devicesInUse, poolSwitchers);

        auto maybeProf = config.getStartProfile();
        if (!maybeProf) {
            LOG(WARNING) << "no start profile configured";
            return;
        }
        auto &prof = maybeProf.value();

        launchProfile(config, prof);
    }

    void Application::launchProfile(const Config &config, Config::Profile &prof) {
        //TODO: this function is basically the constructor of a not yet existent "Profile" class
        using namespace configUtils;

        //clear old state
        algorithms.clear(); //algos call into pools => clear algos before pools!

        //launch profile
        auto &allIds = compute->getAllDeviceIds();

        //find all Algo Implementations that need to be launched
        auto allRequiredImplNames = getUniqueAlgoImplNamesForProfile(prof, allIds);

        LOG(INFO) << "starting profile '" << prof.name << "'";

        auto lockedPoolSwitchers = poolSwitchers.lock();
        lockedPoolSwitchers->clear();

        //for all algorithms that are required to be launched
        for (auto &implName : allRequiredImplNames) {

            auto algoName = Algorithm::implNameToAlgoName(implName);
            auto configPools = getConfigPoolsForAlgoName(config, algoName);

            auto poolSwitcher = std::make_unique<PoolSwitcher>();

            for (auto &configPool : configPools) {
                auto &p = configPool.get();

                auto poolRecords = make_unique<PoolRecords>();

                PoolConstructionArgs args {
                    p.host, p.port, p.username, p.password, *poolRecords
                };

                auto pool = Pool::makePool(args, algoName, p.protocol);

                if (!pool) {
                    LOG(ERROR) << "no pool implementation available for algo type "
                               << algoName << " in combination with protocol type "
                               << stringFromProtoEnum(p.protocol);
                    continue;
                }

                LOG(INFO) << "launching pool '" << pool->getImplName() << "' to connect to " << p.host << " on port " << p.port;

                poolSwitcher->push(std::move(pool), std::move(poolRecords), args);
            }

            if (poolSwitcher->poolCount() == 0) {
                LOG(ERROR) << "no pools available for algoType of " << implName << ". Cannot launch algorithm";
            }

            lockedPoolSwitchers->emplace(algoName, std::move(poolSwitcher));
            MI_ENSURES(lockedPoolSwitchers->count(algoName));

            decltype(AlgoConstructionArgs::assignedDevices) assignedDeviceRefs;

            {
                auto devicesInUseLocked = devicesInUse.lock();
                assignedDeviceRefs = prepareAssignedDevicesForAlgoImplName(implName, config, prof, *devicesInUseLocked,
                                                                           allIds);
            }

            logLaunchInfo(implName, assignedDeviceRefs);

            {
                auto lockedPoolSwitchers = poolSwitchers.readLock();
                AlgoConstructionArgs args{
                        *compute,
                        assignedDeviceRefs,
                        *lockedPoolSwitchers->at(algoName)
                };

                auto algo = Algorithm::makeAlgo(args, implName);

                algorithms.emplace_back(std::move(algo));
            }
        }
    }

    void Application::logLaunchInfo(const std::string &implName, std::vector<std::reference_wrapper<Device>> &assignedDevices) const {
        std::string logText = "launching algorithm '" + implName + "' with devices: ";
        for (auto &d : assignedDevices) {
            logText += "#" + std::to_string(d.get().deviceIndex) + " " + gsl::to_string(d.get().id.getName());
            if (&d != &assignedDevices.back())
                logText += ", ";
        }
        LOG(INFO) << logText;
    }

}









