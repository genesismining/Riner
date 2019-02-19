
#pragma once

#include "AutoRefillQueue.h"
#include "Pool.h"
#include "WorkCuckaroo.h"

#include <src/pool/WorkEthash.h>
#include <src/network/TcpJsonProtocolUtil.h>
#include <src/network/TcpJsonRpcProtocolUtil.h>
#include <src/application/Config.h>
#include <src/util/LockUtils.h>
#include <src/pool/WorkQueue.h>
#include <src/common/Pointers.h>
#include <atomic>
#include <vector>

namespace miner {

    struct GrinStratumProtocolData : public WorkProtocolData {
        GrinStratumProtocolData(uint64_t poolUid) : WorkProtocolData(poolUid) {}

        std::string jobId;
    };

    class PoolGrinStratum : public WorkProvider {
    public:
        explicit PoolGrinStratum(PoolConstructionArgs);
        ~PoolGrinStratum() override;

        cstring_span getName() const override;

        // WorkProvider interface
        optional<unique_ptr<WorkBase>> tryGetWork() override;

        void submitWork(unique_ptr<WorkResultBase> result) override;
    private:
        using QueueItem = std::unique_ptr<Work<kCuckatoo31>>;
        using WorkQueue = AutoRefillQueue<QueueItem>;

        uint64_t getPoolUid() const override;
        void onMiningNotify (const nl::json &jparams);
        void restart();

        const PoolConstructionArgs args_;
        const uint64_t uid;
        std::unique_ptr<WorkQueue> workQueue;

        std::atomic<bool> shutdown {false};

        std::vector<std::shared_ptr<GrinStratumProtocolData>> protocolDatas;

        TcpJsonRpcProtocolUtil jrpc;

        std::unique_ptr<TcpJsonProtocolUtil> tcp;

        bool acceptMiningNotify = false;
        int64_t currentHeight = -1;
    };

}
