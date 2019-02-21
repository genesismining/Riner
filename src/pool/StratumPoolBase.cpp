#include "StratumPoolBase.h"

#include <src/util/Logging.h>
#include <src/util/HexString.h>
#include <src/common/Json.h>
#include <src/common/Endian.h>
#include <src/network/JrpcBuilder.h>
#include <chrono>
#include <random>
#include <functional>

#include <asio.hpp>

namespace miner {

namespace {

constexpr int kRefillThreshold = 8; //once the queue size goes below this, lambda gets called

}

void StratumPoolBase::restart() {
    acceptMiningNotify = false;

    auto authorize = std::make_shared<JrpcBuilder>();
    auto subscribe = std::make_unique<JrpcBuilder>();

    subscribe->method("mining.subscribe")
            .param("sgminer")
            .param("5.5.17-gm");

    subscribe->onResponse([this, authorize] (const JrpcResponse& ret) {
        if (!ret.error() && ret.atEquals("result", true)) {
            jrpc.call(*authorize);
        }
    });

    authorize->method("mining.authorize")
            .param(args.username)
            .param(args.password);

    authorize->onResponse([this] (const JrpcResponse& ret) {
        if (!ret.error()) {
            acceptMiningNotify = true;
        }
    });

    //set handler for receiving incoming messages that are not responses to sent rpcs
    jrpc.setOnReceiveUnhandled([this] (auto &ret) {
        if (acceptMiningNotify &&
                ret.atEquals("method", "mining.notify")) {
            onMiningNotify(ret.getJson());
        }
        else if (ret.getJson().count("method")) {
            //unsupported method
            jrpc.respond( {ret.id(), {JrpcError::method_not_found, "method not supported"}});
        }
        else {
            LOG(INFO) << "unhandled response: " << ret.getJson().dump();
        }
    });

    //set handler that gets called on any received response, before the other handlers are called
    jrpc.setOnReceive([this] (auto &ret) {
            onStillAlive(); //set latest still-alive timestamp to now
        });

    jrpc.call(*subscribe);
}

void StratumPoolBase::onMiningNotify(const nl::json &j) {
    if (!j.count("params") && j.at("params").size() < 5) {
        return;
    }
    nl::json jparams = j.at("params");

    bool cleanFlag = jparams.at(4);
    cleanFlag = true;
    if (cleanFlag) {
        protocolDatas.clear(); //invalidates all gpu work that links to them
    }

    //create work package
    protocolDatas.emplace_back(std::make_shared<StratumProtocolData>(getPoolUid()));
    std::shared_ptr<StratumProtocolData>& sharedProtoData = protocolDatas.back();
    std::weak_ptr<StratumProtocolData> weakProtoData = make_weak(sharedProtoData);

    std::unique_ptr<WorkBase> work = parseWork(weakProtoData, jparams);
    workQueue->setMaster(std::move(work), cleanFlag);
}

void StratumPoolBase::submitWork(unique_ptr<WorkResultBase> result) {
    //build and send submitMessage on the tcp thread

    jrpc.postAsync([this, result = std::move(result)] {
        auto protoData = result->tryGetProtocolDataAs<StratumProtocolData>();
        if (!protoData) {
            LOG(INFO) << "work result cannot be submitted because it has expired";
            return; //work has expired
        }

        std::vector<std::string> powParams = resultToPowParams(*result);

        auto submit = std::make_shared<JrpcBuilder>();

        submit->method("mining.submit")
        .param(args.username)
        .param(protoData.value()->jobId);
        for(auto& powParam: powParams) {
            submit->param(powParam);
        }

        submit->onResponse([] (auto &ret) {
                    auto idStr = ret.id() ? std::to_string(ret.id().value()) : "<no id>";
                    bool accepted = ret.atEquals("result", true);

                    std::string acceptedStr = accepted ? "accepted" : "rejected";
                    LOG(INFO) << "share with id " << idStr << " got " << acceptedStr;
                });

        jrpc.callRetryNTimes(*submit, 5, std::chrono::seconds(5), [submit] {
                    //this handler gets called if there was no response after the last try
                    auto idStr = submit->getId() ? std::to_string(submit->getId().value()) : "<no id>";

                    LOG(INFO) << "share with id " << idStr << " got discarded after pool did not respond multiple times";
                });
    });
}

StratumPoolBase::StratumPoolBase(PoolConstructionArgs args, WorkQueue::RefillFunc refillFunc) :
        args(std::move(args)), uid(createNewPoolUid()), workQueue(
                std::make_unique<WorkQueue>(kRefillThreshold, std::move(refillFunc))), jrpc(args.host, args.port) {
    //jrpc on-restart handler gets called when the connection is first established, and whenever a reconnect happens
    jrpc.setOnRestart([this] {
        restart();
    });
}

StratumPoolBase::~StratumPoolBase() {
    shutdown = true;
}

optional<unique_ptr<WorkBase>> StratumPoolBase::tryGetWork() {
    if (auto work = workQueue->popWithTimeout(std::chrono::milliseconds(100))) {
        return std::move(work.value()); //implicit unique_ptr upcast
    }
    return nullopt; //timeout
}

cstring_span StratumPoolBase::getName() const {
    return args.host;
}

}
