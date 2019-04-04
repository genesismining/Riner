
#pragma once

#include <src/common/Json.h>
#include <src/common/Pointers.h>
#include <src/network/JrpcServer.h>
#include <src/util/LockUtils.h>
#include "Device.h"

namespace miner {

    class ApiServer {
        const LockGuarded<std::vector<optional<Device>>> &devicesInUse;

        unique_ptr<JrpcServer> jrpc;

        void registerFunctions();
    public:
        explicit ApiServer(uint16_t port, const LockGuarded<std::vector<optional<Device>>> &devicesInUse);
        ~ApiServer();
    };

}