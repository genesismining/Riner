
#pragma once

#include <src/common/Json.h>
#include <src/common/Pointers.h>
#include <src/network/JrpcServer.h>
#include <src/util/LockUtils.h>
#include "Device.h"
#include <deque>

namespace miner {
    class PoolSwitcher;

    class ApiServer {
        const LockGuarded<std::deque<optional<Device>>> &devicesInUse;
        const std::array<unique_ptr<PoolSwitcher>, kAlgoTypeCount> &poolSwitchers;

        unique_ptr<JrpcServer> jrpc;

        void registerFunctions();
    public:
        explicit ApiServer(uint16_t port, const LockGuarded<std::deque<optional<Device>>> &devicesInUse, const std::array<unique_ptr<PoolSwitcher>, kAlgoTypeCount> &poolSwitchers);
        ~ApiServer();
    };

}