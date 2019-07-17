
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
#include <src/network/JsonRpcUtil.h>

namespace miner {
    template<class T>
    class AutoRefillQueue;
    class TcpJsonSubscription;

    struct EthashStratumProtocolData : public WorkProtocolData {
        EthashStratumProtocolData(uint64_t poolUid) : WorkProtocolData(poolUid) {
        }

        std::string jobId;
    };

    class PoolEthashStratum : public Pool {
    public:
        explicit PoolEthashStratum(PoolConstructionArgs);
        ~PoolEthashStratum() override;

        cstring_span getName() const override;

    private:
        PoolConstructionArgs args;

        // Pool interface
        optional<unique_ptr<Work>> tryGetWork() override;
        void submitWork(unique_ptr<WorkSolution> result) override;

        using WorkQueueType = AutoRefillQueue<unique_ptr<WorkEthash>>;
        unique_ptr<WorkQueueType> workQueue;

        // Pool Uid
        uint64_t getPoolUid() const override;
        uint64_t uid = createNewPoolUid();

        std::atomic<bool> shutdown {false};

        std::vector<std::shared_ptr<EthashStratumProtocolData>> protocolDatas;

        void onConnected(CxnHandle);
        jrpc::JsonRpcUtil io {IOMode::Tcp};

        CxnHandle _cxn; //modified only on IO thread
        bool acceptMiningNotify = false; //modified only on IO thread

        void onMiningNotify (const nl::json &j);
    };

}