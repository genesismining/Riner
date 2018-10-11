
#include "PoolEthash.h"
#include <src/pool/WorkEthash.h>
#include <src/util/Logging.h>
#include <chrono>
#include <src/network/TcpJsonSubscription.h>
#include <src/common/Json.h>
#include <src/util/HexString.h>
#include <src/pool/AutoRefillQueue.h>
#include <random>

namespace miner {

    PoolEthashStratum::PoolEthashStratum(PoolConstructionArgs args)
    : resultQueue() {

        auto user     = args.username;
        auto password = args.password;

        auto subscribe = [user, password] (std::ostream &stream) {
            stream << R"({"jsonrpc": "2.0", "method" : "mining.subscribe", "params" : {"username": ")"
                   << user << R"(", "password": ")" << password << R"("}, "id": 1})" << "\n";
        };

        tcpJson = std::make_unique<TcpJsonSubscription>(
                args.host, args.port, subscribe, [this] (auto &json) {

            auto m = json.find("method");
            if (m != json.end() && *m == "mining.notify") {

                LOG(INFO) << "received json: " << json.dump(4);

                auto jparams = json["params"];

                bool cleanFlag = jparams[4];
                if (cleanFlag)
                    protocolDatas.clear(); //invalidates all gpu work that links to them

                //create work package
                protocolDatas.emplace_back(std::make_shared<EthashStratumProtocolData>());
                auto &sharedProtoData = protocolDatas.back();
                auto weakProtoData = make_weak(sharedProtoData);

                auto work = std::make_unique<Work<kEthash>>(weakProtoData);

                HexString{jparams[0]}.getBytes(sharedProtoData->jobId);
                HexString{jparams[1]}.getBytes(work->header);
                HexString{jparams[2]}.getBytes(work->seedHash);
                HexString{jparams[3]}.flipByteOrder().getBytes(work->target);

                workQueue->setMaster(std::move(work), cleanFlag);

                auto spacing = "                                                            ";
                LOG(INFO) << spacing <<
                          "<-- pushed new work master";
            }
        });


        //initialize workQueue with refill callback
        auto refillThreshold = 8; //once the queue size goes below this, lambda gets called asap

        workQueue = std::make_unique<WorkQueueType>(refillThreshold,
                [] (auto &out, auto &workMaster, size_t currentSize) {

            for (auto i = currentSize; i < 16; ++i) {
                ++workMaster->extraNonce;
                auto newWork = std::make_unique<Work<kEthash>>(*workMaster);
                out.push_back(std::move(newWork));
            }
            LOG(INFO) << "workQueue got refilled with " << out.size() << " new items";
        });
    }

    PoolEthashStratum::~PoolEthashStratum() {
        shutdown = true;
    }

    optional<unique_ptr<WorkBase>> PoolEthashStratum::tryGetWork() {

        auto timeout = std::chrono::milliseconds(100); //basically the shutdown check interval no work is available

        if (auto work = workQueue->popWithTimeout(timeout))
            return std::move(work.value()); //implicit unique_ptr upcast
        return nullopt; //timeout
    }

    void PoolEthashStratum::submitWork(unique_ptr<WorkResultBase> result) {
        resultQueue.pushBack(std::move(result));
    }

    uint64_t PoolEthashStratum::getPoolUid() const {
        return uid;
    }

}