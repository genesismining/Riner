//
//

#include "TcpJsonRpcProtocolUtil.h"
#include <utility>
#include <src/common/Assert.h>

namespace miner {



    TcpJsonRpcProtocolUtil::TcpJsonRpcProtocolUtil(cstring_span host, cstring_span port) {
        tcpJson = std::make_unique<TcpJsonProtocolUtil>(host, port, [this]
                (auto responseJson, auto &error, auto &coro) {
            onJsonEvent(std::move(responseJson), error, coro);
        });

        tcpJson->launch();
    }

    void TcpJsonRpcProtocolUtil::removeOutdatedPendingRpcs() {
        auto now = std::chrono::system_clock::now();
        auto timeout = std::chrono::minutes(5);

        for (auto it = pendingRpcs.begin(); it != pendingRpcs.end();) {
            if (now - it->second.creationTime > timeout) {
                it = pendingRpcs.erase(it);
            } else {
                ++it;
            }
        }
    }

    void TcpJsonRpcProtocolUtil::onJsonEvent(nl::json json, const asio::error_code &, asio::coroutine &) {

        if (json.empty()) {
            if (!isStarted) {
                isStarted = true;
                onRestartFunc();
            }
        }
        else {
            if (incomingJsonModifierFunc) {//if modifier was set, apply it
                incomingJsonModifierFunc(json);
            }

            bool msgHandled = false;
            JrpcResponse res(json);

            bool isResponse = res.error() || res.result();

            if (res.id() && isResponse) {

                auto it = pendingRpcs.find(res.id().value());
                if (it != pendingRpcs.end()) {
                    JrpcBuilder &rpc = it->second.rpc;

                    rpc.callResponseFunc(res);

                    pendingRpcs.erase(it);
                    msgHandled = true;
                }

            }

            onReceiveFunc(res);

            if (!msgHandled) {
                onReceiveUnhandledFunc(res);
            }
        }

        removeOutdatedPendingRpcs();

        tcpJson->asyncRead();
    }

    void TcpJsonRpcProtocolUtil::setOnReceiveUnhandled(OnReceiveFunc &&func) {
        onReceiveUnhandledFunc = std::move(func);
    }

    void TcpJsonRpcProtocolUtil::setOnReceive(OnReceiveFunc &&func) {
        onReceiveFunc = std::move(func);
    }

    void TcpJsonRpcProtocolUtil::setOnRestart(std::function<void()> &&func) {
        onRestartFunc = std::move(func);
    }

    void TcpJsonRpcProtocolUtil::setOutgoingJsonModifier(JsonModifierFunc &&func) {
        outgoingJsonModifierFunc = std::move(func);
    }

    void TcpJsonRpcProtocolUtil::setIncomingJsonModifier(JsonModifierFunc &&func) {
        incomingJsonModifierFunc = std::move(func);
    }

    void TcpJsonRpcProtocolUtil::call(JrpcBuilder rpc) {
        assignIdIfNecessary(rpc);

        asyncWriteJson(rpc.getJson());

        auto id = rpc.getId().value();
        MI_EXPECTS(pendingRpcs.count(id) == 0); //id should not exist yet

        pendingRpcs[id] = {
            std::chrono::system_clock::now(),
            std::move(rpc)
        };
    }

    void TcpJsonRpcProtocolUtil::callRetryNTimes(JrpcBuilder rpc, uint32_t triesLimit,
                                                 std::chrono::milliseconds retryInterval,
                                                 std::function<void()> neverRespondedHandler) {
        assignIdIfNecessary(rpc);

        int tries = 0;
        auto retryFunc = [this, rpc = std::move(rpc), tries] () mutable {
            MI_EXPECTS(rpc.getId());
            auto id = rpc.getId().value();

            if (tries == 0) {
                call(rpc); //starts id tracking
            }

            bool stillPending = pendingRpcs.count(id);

            if (tries >= 5 || !stillPending) {
                pendingRpcs.erase(id); //remove if it wasn't already removed
                return true; //don't retry
            }

            if (tries > 0) {//tracking already started, just resend the string
                LOG(INFO) << "retrying to send share with id " << id
                          << " (try #" << (tries + 1) << ")";
                asyncWriteJson(rpc.getJson());
            }
            ++tries;

            return false; //retry later
        };

        tcpJson->asyncRetryEvery(retryInterval, retryFunc);
    }

    void TcpJsonRpcProtocolUtil::assignIdIfNecessary(JrpcBuilder &rpc) {
        if (!rpc.getId()) {
            rpc.id(++highestUsedIdYet);
        }
        MI_ENSURES(rpc.getId());
    }

    void TcpJsonRpcProtocolUtil::respond(const JrpcResponse &response) {
        asyncWriteJson(response.getJson());
    }

    void TcpJsonRpcProtocolUtil::asyncWriteJson(const nl::json &jin) {
        if (outgoingJsonModifierFunc) { //if a modify function was set, copy the json and modify it before sending
            nl::json j = jin;
            outgoingJsonModifierFunc(j);
            tcpJson->asyncWrite(j);
        }
        else {
            tcpJson->asyncWrite(jin);
        }
    }

}