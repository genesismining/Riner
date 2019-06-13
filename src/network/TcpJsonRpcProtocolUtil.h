
#pragma once

#include <functional>
#include <src/network/JrpcBuilder.h>
#include <src/common/Pointers.h>
#include <src/network/TcpJsonProtocolUtil.h>
#include <asio.hpp>
#include <map>

namespace miner {
    class TcpJsonProtocolUtil;

    class TcpJsonRpcProtocolUtil {
    public:
        using OnReceiveFunc = std::function<void(const JrpcResponse &j)>;
        using JsonModifierFunc = std::function<void(nl::json &)>;

        explicit TcpJsonRpcProtocolUtil(cstring_span host, uint16_t port);

        void setOnRestart(std::function<void()> &&);

        void callRetryNTimes(JrpcBuilder rpc, uint32_t numTries,
                             std::chrono::milliseconds retryInterval,
                             std::function<void()> neverRespondedHandler);

        //set a callback that is called whenever a json rpc call/notification/response
        //is received that is not a response to a pending call
        void setOnReceiveUnhandled(OnReceiveFunc &&);

        void setOnReceive(OnReceiveFunc &&);

        void setOutgoingJsonModifier(JsonModifierFunc &&);
        void setIncomingJsonModifier(JsonModifierFunc &&);

        void call(JrpcBuilder);
        void respond(const JrpcResponse &);

        template<class Fn>
        void postAsync(Fn &&func) {
            tcpJson->postAsync(std::forward<Fn>(func));
        }

        void assignIdIfNecessary(JrpcBuilder &);

    private:
        std::function<void()> onRestartFunc; //might not be initialized
        OnReceiveFunc onReceiveUnhandledFunc; //might not be initialized
        OnReceiveFunc onReceiveFunc; //might not be initialized

        JsonModifierFunc outgoingJsonModifierFunc; //might not be initialized
        JsonModifierFunc incomingJsonModifierFunc; //might not be initialized

        int highestUsedIdYet = 0;

        void removeOutdatedPendingRpcs();

        void asyncWriteJson(const nl::json &);

        void onJsonEvent(nl::json responseJson, const asio::error_code &, asio::coroutine &);

        struct PendingRpc {
            std::chrono::system_clock::time_point creationTime;
            JrpcBuilder rpc;
        };

        bool isStarted = false;

        std::map<int, PendingRpc> pendingRpcs;

        unique_ptr<TcpJsonProtocolUtil> tcpJson;
    };

}