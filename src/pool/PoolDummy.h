//
//

#pragma once

#include <src/pool/Pool.h>
#include <src/pool/WorkEthash.h>
#include <src/config/Config.h>
#include <src/util/LockUtils.h>
#include <src/common/Pointers.h>
#include <queue>
#include <future>
#include <list>
#include <atomic>
#include <src/network/JsonRpcUtil.h>
#include "WorkQueue.h"

namespace miner {

    class PoolDummy : public Pool {
    public:
        explicit PoolDummy(const PoolConstructionArgs &);
        ~PoolDummy() override;

    private:
        WorkQueue queue;

        // Pool interface
        bool isExpiredJob(const PoolJob &job) override;
        unique_ptr<Work> tryGetWorkImpl() override;
        void submitSolutionImpl(unique_ptr<WorkSolution>) override;
        void onDeclaredDead() override;

        jrpc::JsonRpcUtil io; //io utility object
        CxnHandle _cxn; //io connection to pool

        void onConnected(CxnHandle cxn);
        void tryConnect();

        bool acceptWorkMessages = false;

        void onMiningNotify (const nl::json &j);
    };

    struct EthashDummyJob : public PoolJob {

        const std::string jobId;
        WorkEthash workTemplate;

        std::unique_ptr<Work> makeWork() override { //makeWork is called from another thread (that may or may not be a WorkQueue thread).
            workTemplate.setEpoch();
            workTemplate.extraNonce++;
            return make_unique<WorkEthash>(workTemplate);
        }

        ~EthashDummyJob() override = default;

        explicit EthashDummyJob(const std::weak_ptr<Pool> &pool, std::string id)
                : PoolJob(pool)
                , jobId(std::move(id))
                , workTemplate(0) {
        }

    };

}