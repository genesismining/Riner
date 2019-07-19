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
#include <src/network/JsonRpcUtil.h>
#include "AutoRefillQueue.h"

namespace miner {

    struct EthashDummyProtocolData : public WorkProtocolData {
        using WorkProtocolData::WorkProtocolData;

        std::string jobId;
        std::string someExtraData;
    };

    class PoolDummy : public Pool {
    public:
        explicit PoolDummy(PoolConstructionArgs);
        ~PoolDummy() override;

    private:
        std::atomic_bool shutdown {false};
        PoolConstructionArgs args;

        // Pool interface
        optional<unique_ptr<Work>> tryGetWork() override;
        void submitWorkImpl(unique_ptr<WorkSolution>) override;

        // Work Queue
        using WorkQueue = AutoRefillQueue<unique_ptr<WorkEthash>>;
        unique_ptr<WorkQueue> workQueue;

        // Pool Uid
        const uint64_t uid = createNewPoolUid();
        uint64_t getPoolUid() const override {
            return uid;
        }

        std::vector<shared_ptr<EthashDummyProtocolData>> protocolDatas;

        jrpc::JsonRpcUtil io; //io utility object
        CxnHandle _cxn; //io connection to pool

        void onConnected(CxnHandle cxn);

        bool acceptWorkMessages = false;

        void onMiningNotify (const nl::json &j);
    };

}