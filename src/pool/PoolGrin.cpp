
#include "PoolGrin.h"

#include <src/util/Logging.h>
#include <src/util/HexString.h>
#include <src/common/Json.h>
#include <src/common/Endian.h>
#include <chrono>
#include <random>
#include <functional>
#include <asio.hpp>

namespace miner {

    void PoolGrinStratum::restart() {

        acceptMiningNotify = false;

        auto login = std::make_shared<JrpcBuilder>();
        auto getjobtemplate = std::make_shared<JrpcBuilder>();

        login->method("login")
            .param("agent", "grin-miner")
            .param("login", args_.username)
            .param("pass", args_.password);

        login->onResponse([this, getjobtemplate] (const JrpcResponse& ret) {
            if (!ret.error()) {
                jrpc.call(*getjobtemplate);
            }
        });

        getjobtemplate->method("getjobtemplate");

        getjobtemplate->onResponse([this] (auto &ret) {
            if (!ret.error()) {
                acceptMiningNotify = true;
            }
        });

        //set handler for receiving incoming messages that are not responses to sent rpcs
        jrpc.setOnReceiveUnhandled([this] (const JrpcResponse& ret) {
            if (acceptMiningNotify &&
                ret.atEquals("method", "job")) {
                onMiningNotify(ret.getJson());
            }
            else if (ret.getJson().count("method")) {
                //unsupported method
                jrpc.respond({ret.id(), {JrpcError::method_not_found, "method not supported"}});
            }
            else {
                LOG(INFO) << "unhandled response: " << ret.getJson().dump();
            }
        });

        //set handler that gets called on any received response, before the other handlers are called
        jrpc.setOnReceive([this] (auto &ret) {
            onStillAlive(); //set latest still-alive timestamp to now
        });

        jrpc.call(*login);
    }

    void PoolGrinStratum::onMiningNotify(const nl::json &j) {
        nl::json jparams = j.at("params");

        // TODO clear old jobs only if height changes
        bool cleanFlag = true;
        if (cleanFlag) {
            protocolDatas.clear(); //invalidates all gpu work that links to them
        }

        //create work package
        protocolDatas.emplace_back(std::make_shared<GrinStratumProtocolData>(getPoolUid()));
        auto &sharedProtoData = protocolDatas.back();
        auto weakProtoData = make_weak(sharedProtoData);

        auto work = std::make_unique<Work<kCuckaroo31>>(weakProtoData);

        sharedProtoData->jobId = jparams.at("job_id");
        work->difficulty = jparams.at("difficulty");
        work->height = jparams.at("height");
        work->prePow = HexString(jparams.at("pre_pow"));

        workQueue->setMaster(std::move(work), cleanFlag);
    }

    void PoolGrinStratum::submitWork(unique_ptr<WorkResultBase> resultBase) {
        auto result = static_unique_ptr_cast<WorkResult<kCuckaroo31>>(std::move(resultBase));

        //build and send submitMessage on the tcp thread

        jrpc.postAsync([this, result = std::move(result)] {

            std::shared_ptr<GrinStratumProtocolData> protoData = result->tryGetProtocolDataAs<GrinStratumProtocolData>();
            if (!protoData) {
                LOG(INFO) << "work result cannot be submitted because it has expired";
                return; //work has expired
            }

            auto submit = std::make_shared<JrpcBuilder>();

            nl::json pow;
            for(uint32_t i: result->pow) {
                pow.push_back(i);
            }

            submit->method("submit")
                .param("edge_bits", 31)
                .param("height", result->height)
                .param("job_id", protoData->jobId)
                .param("nonce")
                .param("pow", pow);

            submit->onResponse([] (const JrpcResponse& ret) {
                auto idStr = ret.id() ? std::to_string(ret.id().value()) : "<no id>";
                bool accepted = ret.atEquals("result", "ok");

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

    PoolGrinStratum::PoolGrinStratum(PoolConstructionArgs args)
    : args_(std::move(args))
    , uid(createNewPoolUid())
    , jrpc(args_.host, args_.port) {
        //initialize workQueue
        auto refillThreshold = 8; //once the queue size goes below this, lambda gets called

        auto refillFunc = [] (std::vector<QueueItem>& out, QueueItem& workMaster, size_t currentSize) {
            for (auto i = currentSize; i < 16; ++i) {
                ++workMaster->nonce;
                out.push_back(std::make_unique<Work<kCuckaroo31>>(*workMaster));
            }
            LOG(INFO) << "workQueue got refilled from " << currentSize << " to " << currentSize + out.size() << " items";
        };

        workQueue = std::make_unique<WorkQueue>(refillThreshold, refillFunc);

        //jrpc on-restart handler gets called when the connection is first established, and whenever a reconnect happens
        jrpc.setOnRestart([this] {
            restart();
        });
    }

    PoolGrinStratum::~PoolGrinStratum() {
        shutdown = true;
    }

    optional<unique_ptr<WorkBase>> PoolGrinStratum::tryGetWork() {
        if (auto work = workQueue->popWithTimeout(std::chrono::milliseconds(100))) {
            return std::move(work.value()); //implicit unique_ptr upcast
        }
        return nullopt; //timeout
    }

    uint64_t PoolGrinStratum::getPoolUid() const {
        return uid;
    }

    cstring_span PoolGrinStratum::getName() const {
        return args_.host;
    }
}
