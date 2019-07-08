
#pragma once

#include <src/common/Json.h>
#include <src/common/Pointers.h>
#include <src/network/JrpcServer.h>
#include <src/util/LockUtils.h>
#include "Device.h"
#include <deque>
#include <map>

namespace miner {
    class PoolSwitcher;

    class ApiServer {
        const SharedLockGuarded<std::deque<optional<Device>>> &devicesInUse;
        const SharedLockGuarded<std::map<std::string, unique_ptr<PoolSwitcher>>> &poolSwitchers;

        unique_ptr<JrpcServer> jrpc;

        void registerFunctions();
    public:
        explicit ApiServer(uint16_t port, const SharedLockGuarded<std::deque<optional<Device>>> &devicesInUse,
                const SharedLockGuarded<std::map<std::string, unique_ptr<PoolSwitcher>>> &poolSwitchers);
        ~ApiServer();
    };

}