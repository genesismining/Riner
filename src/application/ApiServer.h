
#pragma once

#include <src/common/Json.h>
#include <src/common/Pointers.h>
#include <src/util/LockUtils.h>
#include <src/network/JsonRpcUtil.h>
#include "Device.h"
#include <deque>
#include <map>


namespace miner {
    class PoolSwitcher;

    class ApiServer {
        const SharedLockGuarded<std::deque<opt::optional<Device>>> &devicesInUse;
        const SharedLockGuarded<std::map<std::string, unique_ptr<PoolSwitcher>>> &poolSwitchers;

        unique_ptr<jrpc::JsonRpcUtil> io;

        void registerFunctions();
    public:
        explicit ApiServer(uint16_t port, const SharedLockGuarded<std::deque<opt::optional<Device>>> &devicesInUse,
                const SharedLockGuarded<std::map<std::string, unique_ptr<PoolSwitcher>>> &poolSwitchers);
        ~ApiServer();
    };

}
