//
//

#pragma once

#include <src/pool/Pool.h>
#include <src/pool/WorkEthash.h>
#include <src/application/Config.h>
#include <src/util/LockUtils.h>
#include <src/common/Pointers.h>
#include <queue>
#include <future>
#include <list>
#include <atomic>

namespace miner {
    template<class T>
    class AutoRefillQueue;
    class TcpJsonSubscription;

    /*
    struct DummyProtocolData : public DummyProtocolData {
        DummyProtocolData(uint64_t poolUid) : WorkProtocolData(poolUid) {
        }

        std::string jobId;
    };

    class PoolDummy : public Pool {
    public:
        explicit PoolDummy(PoolConstructionArgs);
        ~PoolDummy() override;


    private:
        PoolConstructionArgs args;

        // Pool interface
        optional<unique_ptr<WorkBase>> tryGetWork() override;
        void submitWorkImpl(unique_ptr<WorkSolution> result) override;

        using WorkQueueType = AutoRefillQueue<unique_ptr<WorkEthash>>;
        unique_ptr<WorkQueueType> workQueue;

        // Pool Uid
        uint64_t getPoolUid() const override;
        uint64_t uid = createNewPoolUid();

        std::atomic<bool> shutdown {false};

        std::vector<std::shared_ptr<DummyProtocolData>> protocolDatas;

        void restart();
        TcpJsonRpcProtocolUtil jrpc;

        bool acceptMiningNotify = false;

        void onMiningNotify (const nl::json &j);
    };
*/
}