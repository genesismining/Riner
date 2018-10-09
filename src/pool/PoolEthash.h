
#pragma once

#include <src/pool/Pool.h>
#include <src/pool/WorkEthash.h>
#include <src/network/TcpJsonSubscription.h>
#include <src/application/Config.h>
#include <src/util/LockUtils.h>
#include <src/pool/WorkQueue.h>
#include <queue>
#include <future>
#include <atomic>

namespace miner {

    class PoolEthashStratum : public WorkProvider {
    public:
        PoolEthashStratum();
        ~PoolEthashStratum();

    protected:
        optional<unique_ptr<WorkBase>> tryGetWork() override;

        void submitWork(unique_ptr<WorkResultBase> result) override;

        uint64_t getPoolUid() const override;

    private:
        std::atomic<bool> shutdown {false};

        uint64_t uid = createNewPoolUid();

        unique_ptr<TcpJsonSubscription> tcpJson;

        WorkQueue<unique_ptr<WorkBase>> workQueue {16};
        WorkQueue<unique_ptr<WorkResultBase>> resultQueue;

        std::future<void> task;
    };

}