
#include "PoolEthash.h"
#include <src/pool/WorkEthash.h>
#include <src/util/Logging.h>
#include <chrono>

namespace miner {

    PoolEthashStratum::PoolEthashStratum()
    : workQueue(16)
    , resultQueue() {

        task = std::async([this] {
            std::vector<std::shared_ptr<WorkProtocolData>> pds;

            auto waitedSecs = 0;
            while(!shutdown) {
                std::this_thread::sleep_for(std::chrono::milliseconds(1000));
                ++waitedSecs;

                pds.emplace_back(std::make_shared<WorkProtocolData>());
                auto wp = make_weak(pds.back());

                size_t size = 0;
                for (size_t i = 0; i < 8; ++i) {
                    auto work = std::make_unique<Work<kEthash>>(wp);

                    size = workQueue.pushBack(std::move(work));
                }

                auto spacing = "                                                            ";
                LOG(INFO) << spacing <<
                "<-- pushed new work (total: " << size << ")";

                if (waitedSecs == 20) {
                    LOG(INFO) << spacing << "<-- made all work expire";
                    workQueue.clear();
                    pds.clear(); //delete protocol data => makes all work expire
                }
            }

        });

    }

    PoolEthashStratum::~PoolEthashStratum() {
        shutdown = true;
    }

    optional<unique_ptr<WorkBase>> PoolEthashStratum::tryGetWork() {
        return workQueue.popBackTimeout();
    }

    void PoolEthashStratum::submitWork(unique_ptr<WorkResultBase> result) {
        resultQueue.pushBack(std::move(result));
    }

    uint64_t PoolEthashStratum::getPoolUid() const {
        return uid;
    }

}