
#pragma once

#include <src/pool/Pool.h>
#include <src/pool/WorkEthash.h>
#include <src/application/Config.h>
#include <src/util/LockUtils.h>
#include <src/pool/WorkQueue.h>
#include <src/common/Pointers.h>
#include <queue>
#include <future>
#include <atomic>

namespace miner {
    template<class T>
    class AutoRefillQueue;
    class TcpJsonSubscription;

    struct EthashStratumProtocolData : public WorkProtocolData {
        Bytes<32> jobId {};
        bool clean = false;
        //nl::json jobId;
    };

    class PoolEthashStratum : public WorkProvider {
    public:
        explicit PoolEthashStratum(PoolConstructionArgs);
        ~PoolEthashStratum() override;

    private:
        optional<unique_ptr<WorkBase>> tryGetWork() override;

        void submitWork(unique_ptr<WorkResultBase> result) override;

        uint64_t getPoolUid() const override;

        std::atomic<bool> shutdown {false};

        uint64_t uid = createNewPoolUid();

        unique_ptr<TcpJsonSubscription> tcpJson;

        std::vector<std::shared_ptr<EthashStratumProtocolData>> protocolDatas;

        using WorkQueueType = AutoRefillQueue<unique_ptr<Work<kEthash>>>;
        unique_ptr<WorkQueueType> workQueue;

        WorkQueue<unique_ptr<WorkResultBase>> resultQueue;
    };

}