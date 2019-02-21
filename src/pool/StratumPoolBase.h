
#pragma once

#include "AutoRefillQueue.h"
#include "Pool.h"
#include "Work.h"

#include <src/network/TcpJsonProtocolUtil.h>
#include <src/network/TcpJsonRpcProtocolUtil.h>
#include <src/application/Config.h>
#include <src/util/LockUtils.h>
#include <src/pool/WorkQueue.h>
#include <src/common/Pointers.h>
#include <queue>
#include <future>
#include <list>
#include <atomic>
#include <vector>
#include <string>


namespace miner {

    struct StratumProtocolData : public WorkProtocolData {
        StratumProtocolData(uint64_t poolUid) : WorkProtocolData(poolUid) {
        }

        std::string jobId;
    };

    class StratumPoolBase : public WorkProvider {
    public:
        using WorkQueue = AutoRefillQueue<std::unique_ptr<WorkBase>>;

        explicit StratumPoolBase(PoolConstructionArgs args, WorkQueue::RefillFunc refillFunc);
        virtual ~StratumPoolBase() override;

        cstring_span getName() const override;

        // WorkProvider interface
        optional<std::unique_ptr<WorkBase>> tryGetWork() override;

        void submitWork(unique_ptr<WorkResultBase> result) override;

    protected:
        virtual std::unique_ptr<WorkBase> parseWork(std::weak_ptr<StratumProtocolData> protocolData, nl::json params) = 0;
        virtual std::vector<std::string> resultToPowParams(const WorkResultBase& result) = 0;

    private:
        // Pool Uid
        uint64_t getPoolUid() const override {
            return uid;
        }

        void restart();

        void onMiningNotify (const nl::json &j);

        const PoolConstructionArgs args;
        const uint64_t uid;
        const std::unique_ptr<WorkQueue> workQueue;
        std::atomic<bool> shutdown {false};
        std::atomic<bool> acceptMiningNotify {false};

        std::vector<std::shared_ptr<StratumProtocolData>> protocolDatas; // TODO should be protected from concurrent access

        TcpJsonRpcProtocolUtil jrpc;

        unique_ptr<TcpJsonProtocolUtil> tcp;
    };

}
