
#pragma once

#include "WorkQueue.h"
#include "WorkCuckoo.h"

#include <src/pool/WorkEthash.h>
#include <src/network/JsonRpcUtil.h>
#include <src/config/Config.h>
#include <src/util/LockUtils.h>
#include <src/util/Random.h>
#include <src/common/Pointers.h>
#include <atomic>
#include <vector>

namespace miner {

    struct GrinStratumJob : public PoolJob {

        int64_t jobId;
        int64_t height;
        WorkCuckatoo31 workTemplate;

        std::unique_ptr<Work> makeWork() override {
            workTemplate.nonce++;
            return std::make_unique<WorkCuckatoo31>(workTemplate);
        }

        explicit GrinStratumJob(const std::weak_ptr<Pool> &pool, int64_t id, int64_t height)
                : PoolJob(pool)
                , jobId(id)
                , height(height)
                , workTemplate() {
        }
    };

    class PoolGrinStratum : public Pool {
    public:
        explicit PoolGrinStratum(const PoolConstructionArgs &);
        ~PoolGrinStratum() override;

        // Pool interface
        bool isExpiredJob(const PoolJob &job) override;
        unique_ptr <Work> tryGetWorkImpl() override;
        void submitSolutionImpl(unique_ptr<WorkSolution> result) override;
    private:

        void onMiningNotify (const nl::json &jparams);
        void onConnected(CxnHandle);
        void tryConnect();

        void onDeclaredDead() override;

        LazyWorkQueue queue;

        Random random_;
        jrpc::JsonRpcUtil io;
        CxnHandle _cxn; //connection to submit shares to (set on mining notify)

        int64_t currentHeight = -1;
    };

}
