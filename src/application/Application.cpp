
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

namespace miner {

    void Application::parseConfig(miner::cstring_span configPath) {

        auto configStr = file::readFileIntoString(configPath);
        if (!configStr) {
            LOG(ERROR) << "unable to read config file";
            return;
        }

        LOG(INFO) << "parsing config string:\n" << configStr.value();
        Config config(configStr.value());

        optional_ref<const Config::Pool> configPool;
        config.forEachPool([&] (const Config::Pool &pool) {
            configPool = optional_ref<const Config::Pool>(pool);
        });

        if (!configPool) {
            LOG(ERROR) << "no pool in config";
            return;
        }

        auto &pool = configPool.value();

        auto user = pool.username, password = pool.password;
        LOG(INFO) << "user: " << user << ", password: " << password;
    }

    Application::Application(optional<std::string> configPath) {

        if (configPath) {
            parseConfig(configPath.value());
        }
        else {
            LOG(ERROR) << "no config path command line argument (--config /path/to/config.json)";
        }

#if 1
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

        PoolSwitcher poolSwitcher;

        poolSwitcher.emplace<PoolEthashStratum>({
            "eth-eu1.nanopool.org", "9999", username, password
        });

        poolSwitcher.emplace<PoolEthashStratum>({
            host, port, username, password
        });

        //PoolEthashStratum poolEthashStratum({host, port, username, password});

        AlgoEthashCL algo({compute, compute.getAllDeviceIds(), poolSwitcher});

        for (size_t i = 0; i < 60 * 4; ++i) {
            std::this_thread::sleep_for(std::chrono::seconds(1));
            //LOG(INFO) << "...";
        }
        LOG(INFO) << "test period over, closing application";
    }

}









