
#pragma once

#include <src/common/Json.h>
#include <src/common/Pointers.h>
#include <src/util/LockUtils.h>
#include <src/network/JsonRpcUtil.h>
#include "Device.h"
#include "Application.h"
#include <deque>
#include <map>

namespace miner {
    class Application;

    class ApiServer {
        const Application &_app; //app owns the ApiServer, therefore app outlives the Apiserver, which makes this ref safe

        unique_ptr<jrpc::JsonRpcUtil> io;

        void registerFunctions();
    public:
        explicit ApiServer(uint16_t port, const Application &app);
        ~ApiServer();
    };

}
