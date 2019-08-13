
#pragma once

#include "PoolWithWorkQueue.h"
#include "WorkCuckoo.h"

#include <src/pool/WorkEthash.h>
#include <src/network/JsonRpcUtil.h>
#include <src/application/Config.h>
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

        explicit GrinStratumJob(int64_t id, int64_t height)
                : jobId(id)
                , height(height)
                , workTemplate() {
        }
    };

    class PoolGrinStratum : public PoolWithoutWorkQueue {
    public:
        explicit PoolGrinStratum(PoolConstructionArgs);
        ~PoolGrinStratum() override;

        cstring_span getName() const override;

        // Pool interface
        void submitSolutionImpl(unique_ptr<WorkSolution> result) override;
    private:

        void onMiningNotify (const nl::json &jparams);
        void restart();
        void onConnected(CxnHandle);

        const PoolConstructionArgs args_;

        Random random_;
        std::atomic<bool> shutdown {false};
        jrpc::JsonRpcUtil io;
        CxnHandle _cxn; //connection to submit shares to (set on mining notify)

        int64_t currentHeight = -1;
    };

}
