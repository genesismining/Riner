//
//

#include "TcpJsonRpcProtocolUtil.h"
#include <utility>

#include <src/network/TcpJsonProtocolUtil.h>
#include <src/common/Assert.h>

namespace miner {



    TcpJsonRpcProtocolUtil::TcpJsonRpcProtocolUtil(cstring_span host, cstring_span port) {
        tcpJson = std::make_unique<TcpJsonProtocolUtil>(host, port, [this]
                (auto responseJson, auto &error, auto &coro) {
            onJsonEvent(std::move(responseJson), error, coro);
        });
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

        if (!json.empty()) {

            JrpcResponse res(json);

            if (auto id = res.id()) {

                auto it = pendingRpcs.find(id.value());
                if (it != pendingRpcs.end()) {
                    JrpcBuilder &rpc = it->second.rpc;

                    rpc.callResponseFunc(res);

                    pendingRpcs.erase(it);
                }
            }
            else {
                onReceiveFunc(res);
            }
        }

        removeOutdatedPendingRpcs();

        tcpJson->asyncRead();
    }

    void TcpJsonRpcProtocolUtil::setOnReceive(TcpJsonRpcProtocolUtil::OnReceiveFunc &&func) {
        onReceiveFunc = std::move(func);
    }

    void TcpJsonRpcProtocolUtil::setOnRestart(OnRestartFunc &&func ) {
        onRestartFunc = std::move(func);
    }

    void TcpJsonRpcProtocolUtil::call(JrpcBuilder rpc) {
        if (!rpc.getId()) {
            auto id = ++highestUsedIdYet;
            rpc.id(id);

            MI_EXPECTS(pendingRpcs.count(id) == 0); //id should not exist yet

            tcpJson->asyncWrite(rpc.getJson());

            pendingRpcs.emplace(std::make_pair(id, PendingRpc{
                std::chrono::system_clock::now(),
                std::move(rpc)
            }));
        }
    }

    void TcpJsonRpcProtocolUtil::postAsync(std::function<void()> &&func) {
        tcpJson->postAsync(std::move(func));
    }

    void TcpJsonRpcProtocolUtil::callRetryNTimes(JrpcBuilder rpc, uint32_t triesLimit,
            std::chrono::milliseconds retryInterval,
            std::function<void()> neverRespondedHandler) {

            call(rpc);

            int tries = 0;
            auto retryFunc = [this, rpc = std::move(rpc), tries] () mutable {
                MI_EXPECTS(rpc.getId());
                auto id = rpc.getId().value();

                if (tries > 5 || !pendingRpcs.count(id)) {
                    pendingRpcs.erase(id); //remove if it wasn't already removed
                    return true; //don't retry
                }

                if (tries != 0) {//0th try is done within 'call(rpc)' outside of this lambda
                    tcpJson->asyncWrite(rpc.getJson().dump());
                }
                ++tries;
                return false; //retry later
            };

            tcpJson->asyncRetryEvery(retryInterval, retryFunc);
    }


}