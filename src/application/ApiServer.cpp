
#include "ApiServer.h"
#include <numeric>

namespace miner {

    ApiServer::ApiServer(uint64_t port)
            : jrpc(std::make_unique<JrpcServer>(port)) {

        //json exceptions will get caught by the caller of these functions

        jrpc->registerFunc("divide", [] (int a, int b) -> JrpcReturn {

            if (b == 0)
                return {JrpcError::invalid_params, "division by zero"};

            return float(a) / b;

        }, "a", "b");

        jrpc->registerFunc("stats", [&] () -> JrpcReturn {
            return JrpcError::invalid_params;
        });

    }

    ApiServer::~ApiServer() {
        // manually destroy jrpc first, so the registered functions don't access
        // destroyed members asynchronously by accident
        jrpc.reset();
    }

}