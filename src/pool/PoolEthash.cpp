
#include "PoolEthash.h"
#include <src/algorithm/ethash/Ethash.h>
#include <src/pool/WorkEthash.h>
#include <src/pool/AutoRefillQueue.h>
#include <src/util/Logging.h>
#include <src/util/HexString.h>
#include <src/common/Json.h>
#include <src/common/Endian.h>
#include <chrono>
#include <random>
#include <functional>

#include <asio.hpp>
#include <src/common/Chrono.h>

namespace miner {

    void PoolEthashStratum::onConnected(CxnHandle cxn) {
        acceptMiningNotify = false;

        jrpc::Message subscribe = jrpc::RequestBuilder{}
            .id(io.nextId++)
            .method("mining.subscribe")
            .param("sgminer")
            .param("5.5.17-gm")
            .done();

        io.callAsync(cxn, subscribe, [&] (CxnHandle cxn, jrpc::Message response) {
            //return if its not {"result": true} message
            if (!response.resultAs<bool>().value_or(false))
                return;

            jrpc::Message authorize = jrpc::RequestBuilder{}
                .id(io.nextId++)
                .method("mining.authorize")
                .param(args.username)
                .param(args.password)
                .done();

            io.callAsync(cxn, authorize, [&] (CxnHandle cxn, jrpc::Message response) {
                acceptMiningNotify = true;
                _cxn = cxn; //store connection for submit
            });
        });

        io.addMethod("mining.notify", [&] (nl::json params) {
            if (acceptMiningNotify)
                onMiningNotify(params);
        });

        io.setIncomingModifier([&] (jrpc::Message &) {
            //called everytime a jrpc::Message is incoming
            onStillAlive(); //update still alive timer
        });

    }

    void PoolEthashStratum::restart() {

        acceptMiningNotify = false;

        auto authorize = std::make_shared<JrpcBuilder>();
        auto subscribe = std::make_unique<JrpcBuilder>();

        subscribe->method("mining.subscribe")
            .param("sgminer")
            .param("5.5.17-gm");

        subscribe->onResponse([this, authorize] (auto &ret) {
            if (!ret.error() && ret.atEquals("result", true))
                jrpc.call(*authorize);
        });

        authorize->method("mining.authorize")
            .param(args.username)
            .param(args.password);

        authorize->onResponse([this] (auto &ret) {
            if (!ret.error())
                acceptMiningNotify = true;
        });

        //set handler for receiving incoming messages that are not responses to sent rpcs
        jrpc.setOnReceiveUnhandled([this] (auto &ret) {
            if (acceptMiningNotify &&
                ret.atEquals("method", "mining.notify")) {
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

        jrpc.call(*subscribe);
    }

    void PoolEthashStratum::onMiningNotify(const nl::json &j) {
        if (!j.count("params") && j.at("params").size() < 5)
            return;
        auto jparams = j.at("params");

        bool cleanFlag = jparams.at(4);
        cleanFlag = true;
        if (cleanFlag)
            protocolDatas.clear(); //invalidates all gpu work that links to them

        //create work package
        protocolDatas.emplace_back(std::make_shared<EthashStratumProtocolData>(getPoolUid()));
        auto &sharedProtoData = protocolDatas.back();
        auto weakProtoData = make_weak(sharedProtoData);

        auto work = std::make_unique<Work<kEthash>>(weakProtoData);

        sharedProtoData->jobId = jparams.at(0);
        HexString(jparams[1]).getBytes(work->header);
        HexString(jparams[2]).getBytes(work->seedHash);
        HexString(jparams[3]).swapByteOrder().getBytes(work->target);

        //work->epoch is calculated in the refill thread

        workQueue->setMaster(std::move(work), cleanFlag);
    }

    void PoolEthashStratum::submitWork(unique_ptr<WorkResultBase> resultBase) {

        auto result = static_unique_ptr_cast<WorkResult<kEthash>>(std::move(resultBase));

        //build and send submitMessage on the tcp thread

        io.postAsync([this, result = std::move(result)] {

            auto protoData = result->tryGetProtocolDataAs<EthashStratumProtocolData>();
            if (!protoData) {
                LOG(INFO) << "work result cannot be submitted because it has expired";
                return; //work has expired
            }

            uint32_t shareId = io.nextId++;

            jrpc::Message submit = jrpc::RequestBuilder{}
                .id(shareId)
                .method("mining.submit")
                .param(args.username)
                .param(protoData->jobId)
                .param("0x" + HexString(toBytesWithBigEndian(result->nonce)).str()) //nonce must be big endian
                .param("0x" + HexString(result->proofOfWorkHash).str())
                .param("0x" + HexString(result->mixHash).str())
                .done();

            auto onResponse = [] (CxnHandle cxn, jrpc::Message response) {
                bool accepted = response.resultAs<bool>().value_or(false);

                std::string acceptedStr = accepted ? "accepted" : "rejected";
                LOG(INFO) << "share with id '" << response.id << "' got " << acceptedStr;
            };

            //this handler gets called if there was no response after the last try
            auto onNeverResponded = [shareId] () {
                LOG(INFO) << "share with id " << shareId << " got discarded after pool did not respond multiple times";
            };

            io.callAsyncRetryNTimes(_cxn, submit, 5, seconds(5), onResponse, onNeverResponded);

        });
    }
/*
    void PoolEthashStratum::submitWork(unique_ptr<WorkResultBase> resultBase) {

        auto result = static_unique_ptr_cast<WorkResult<kEthash>>(std::move(resultBase));

        //build and send submitMessage on the tcp thread

        jrpc.postAsync([this, result = std::move(result)] {

            std::shared_ptr<EthashStratumProtocolData> protoData =
                    result->tryGetProtocolDataAs<EthashStratumProtocolData>();
            if (!protoData) {
                LOG(INFO) << "work result cannot be submitted because it has expired";
                return; //work has expired
            }

            auto submit = std::make_shared<JrpcBuilder>();

            submit->method("mining.submit")
                .param(args.username)
                .param(protoData->jobId)
                .param("0x" + HexString(toBytesWithBigEndian(result->nonce)).str()) //nonce must be big endian
                .param("0x" + HexString(result->proofOfWorkHash).str())
                .param("0x" + HexString(result->mixHash).str());

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
*/
    PoolEthashStratum::PoolEthashStratum(PoolConstructionArgs args)
    : args(args)
    , jrpc(args.host, args.port) {
        //initialize workQueue
        auto refillThreshold = 8; //once the queue size goes below this, lambda gets called

        auto refillFunc = [] (auto &out, auto &workMaster, size_t currentSize) {

            if (workMaster->epoch == 0) {//calculate epoch for master if it didn't happen yet
                workMaster->epoch = EthCalcEpochNumber(workMaster->seedHash);
            }

            for (auto i = currentSize; i < 16; ++i) {
                ++workMaster->extraNonce;
                auto newWork = std::make_unique<Work<kEthash>>(*workMaster);
                out.push_back(std::move(newWork));
            }
            LOG(INFO) << "workQueue got refilled from " << currentSize << " to " << currentSize + out.size() << " items";
        };

        workQueue = std::make_unique<WorkQueueType>(refillThreshold, refillFunc);

        //launchClient establishes a single connection to the given host + port
        io.launchClient(args.host, args.port, [this] (auto cxn) {
            onConnect(cxn);
        });

        //jrpc on-restart handler gets called when the connection is first established, and whenever a reconnect happens
        jrpc.setOnRestart([this] {
            restart();
        });
    }

    PoolEthashStratum::~PoolEthashStratum() {
        shutdown = true;
    }

    optional<unique_ptr<WorkBase>> PoolEthashStratum::tryGetWork() {
        if (auto work = workQueue->popWithTimeout(std::chrono::milliseconds(100)))
            return std::move(work.value()); //implicit unique_ptr upcast
        return nullopt; //timeout
    }

    uint64_t PoolEthashStratum::getPoolUid() const {
        return uid;
    }

    cstring_span PoolEthashStratum::getName() const {
        return args.host;
    }

}
