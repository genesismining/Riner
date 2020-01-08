
#include <iostream>

#include "Application.h"
#include <src/util/FileUtils.h>
#include <src/util/Logging.h>
#include <src/common/Json.h>
#include <src/util/ConfigUtils.h>
#include <src/application/ApiServer.h>
#include <thread>

namespace miner {

    Application::Application(optional<std::string> configPath) {
        using namespace std::chrono;

        if (configPath) {
            if (auto configOr = configUtils::loadConfig(*configPath))
                config = std::move(*configOr);
            else {
                LOG(ERROR) << "no valid config available";
                return;
            }
        } else {
            LOG(ERROR) << "no config path command line argument (--config /path/to/config.json)";
            return;
        }

        compute = std::make_unique<ComputeModule>(config);

        //init devicesInUse
        devicesInUse.lock()->resize(compute->getAllDeviceIds().size());

        auto port = config.global_settings().api_port();
        apiServer = make_unique<ApiServer>(port, *this);

        auto startProfileOr = getStartProfile(config);
        if (!startProfileOr) {
            LOG(WARNING) << "no start profile configured";
            return;
        }

        launchProfile(config, *startProfileOr);
    }

    Application::~Application() {
        //this dtor only exists for type forward declaration reasons (unique_ptr<T>'s std::default_delete<T>)
    }

    void Application::launchProfile(const Config &config, const proto::Config_Profile &prof) {
        //TODO: this function is basically the constructor of a not yet existent "Profile" class
        using namespace configUtils;

        //clear old state
        algorithms.clear(); //algos call into pools => clear algos before pools!

        //launch profile
        auto &allIds = compute->getAllDeviceIds();

        //find all Algo Implementations that need to be launched
        auto allRequiredImplNames = getUniqueAlgoImplNamesForProfile(prof, allIds);

        LOG(INFO) << "starting profile '" << prof.name() << "'";

        auto lockedPoolSwitchers = poolSwitchers.lock();
        lockedPoolSwitchers->clear();

        //for all algorithms that are required to be launched
        for (auto &implName : allRequiredImplNames) {

            auto powType = Algorithm::powTypeForAlgoImplName(implName);
            auto configPools = getConfigPoolsForPowType(config, powType);

            auto &poolSwitcher = lockedPoolSwitchers->emplace(
                    std::make_pair(powType, std::make_unique<PoolSwitcher>(powType))
                    ).first->second;

            for (auto &configPool : configPools) {
                auto &p = configPool.get();

                MI_EXPECTS(p.port() == (uint32_t)(uint16_t)p.port());
                PoolConstructionArgs args {p.host(), (uint16_t)p.port(), p.username(), p.password()};

                const std::string &poolImplName = Pool::getPoolImplNameForPowAndProtocolType(powType, p.protocol());
                if (poolImplName.empty()) {
                    LOG(ERROR) << "no pool implementation available for algo type "
                               << powType << " in combination with protocol type "
                               << p.protocol();
                    continue;
                }

                LOG(INFO) << "launching pool '" << poolImplName << "' to connect to " << p.host() << " on port " << p.port();

                poolSwitcher->tryAddPool(args, poolImplName);
            }

            if (poolSwitcher->poolCount() == 0) {
                LOG(ERROR) << "no pools available for algoType of " << implName << ". Cannot launch algorithm";
            }

            MI_ENSURES(lockedPoolSwitchers->count(powType));

            decltype(AlgoConstructionArgs::assignedDevices) assignedDeviceRefs;

            {
                auto devicesInUseLocked = devicesInUse.lock();
                assignedDeviceRefs = prepareAssignedDevicesForAlgoImplName(implName, config, prof, *devicesInUseLocked,
                                                                           allIds);
            }

            logLaunchInfo(implName, assignedDeviceRefs);

            AlgoConstructionArgs args{
                    *compute,
                    assignedDeviceRefs,
                    *lockedPoolSwitchers->at(powType) //TODO: do the pool switchers actually need to stay locked while calling into the user's algo and pool ctors?
            };

            auto algo = Algorithm::makeAlgo(args, implName);

            algorithms.emplace_back(std::move(algo));
        }
    }

    void Application::logLaunchInfo(const std::string &implName, std::vector<std::reference_wrapper<Device>> &assignedDevices) const {
        std::string logText = "launching algorithm '" + implName + "' with devices: ";
        for (auto &d : assignedDevices) {
            logText += "#" + std::to_string(d.get().deviceIndex) + " " + d.get().id.getName();
            if (&d != &assignedDevices.back())
                logText += ", ";
        }
        LOG(INFO) << logText;
    }

}









