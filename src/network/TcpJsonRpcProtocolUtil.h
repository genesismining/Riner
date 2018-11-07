
#pragma once

#include <functional>
#include <src/network/JrpcBuilder.h>
#include <src/common/Pointers.h>
#include <asio.hpp>
#include <map>

namespace miner {
    class TcpJsonProtocolUtil;

    class TcpJsonRpcProtocolUtil {
    public:
        using OnRestartFunc = std::function<void()>; //called when reconnected
        using OnReceiveFunc = std::function<void(const JrpcResponse &j)>;

        explicit TcpJsonRpcProtocolUtil(cstring_span host, cstring_span port);


        void setOnRestart(OnRestartFunc &&);

        //set a callback that is called whenever a json rpc call/notification/response
        //is received that is not a response to a pending call
        void setOnReceive(OnReceiveFunc &&);

        void call(JrpcBuilder);

    private:
        OnRestartFunc onRestartFunc; //may not be initialized
        OnReceiveFunc onReceiveFunc; //may not be initialized

        int highestUsedIdYet = 0;

        void postAsync(std::function<void()> &&);

        void removeOutdatedPendingRpcs();

        void onJsonEvent(nl::json responseJson, const asio::error_code &, asio::coroutine &);

        struct PendingRpc {
            std::chrono::system_clock::time_point creationTime;
            JrpcBuilder rpc;
        };

        std::map<int, PendingRpc> pendingRpcs;

        unique_ptr<TcpJsonProtocolUtil> tcpJson;

        void callRetryNTimes(JrpcBuilder rpc, uint32_t numTries,
                std::chrono::milliseconds retryInterval,
                std::function<void()> neverRespondedHandler);
    };

}