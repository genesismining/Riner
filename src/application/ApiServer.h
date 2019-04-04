
#pragma once

#include <src/common/Json.h>
#include <src/common/Pointers.h>
#include <src/network/JrpcServer.h>

namespace miner {

    class ApiServer {
        unique_ptr<JrpcServer> jrpc;

    public:
        explicit ApiServer(uint16_t port);
        ~ApiServer();
    };

}