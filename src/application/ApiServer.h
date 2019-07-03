
#pragma once

#include <src/common/Json.h>
#include <src/common/Pointers.h>
#include <src/util/LockUtils.h>
#include "Device.h"
#include <deque>
#include <src/network/JsonRpcUtil.h>

namespace miner {
    class PoolSwitcher;

    class ApiServer {
        const LockGuarded<std::deque<optional<Device>>> &devicesInUse;
        const std::array<unique_ptr<PoolSwitcher>, kAlgoTypeCount> &poolSwitchers;

        unique_ptr<jrpc::JsonRpcUtil> io;

        void registerFunctions();
    public:
        explicit ApiServer(uint16_t port, const LockGuarded<std::deque<optional<Device>>> &devicesInUse, const std::array<unique_ptr<PoolSwitcher>, kAlgoTypeCount> &poolSwitchers);
        ~ApiServer();
    };

}