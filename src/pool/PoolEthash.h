
#pragma once

#include <src/pool/WorkQueue.h>
#include <src/pool/WorkEthash.h>
#include <src/config/Config.h>
#include <src/util/LockUtils.h>
#include <src/common/Pointers.h>
#include <queue>
#include <future>
#include <list>
#include <atomic>
#include <src/network/JsonRpcUtil.h>

namespace miner {

    struct EthashStratumJob : public PoolJob {

        const std::string jobId;
        WorkEthash workTemplate;

        std::unique_ptr<Work> makeWork() override {
            workTemplate.setEpoch();
            workTemplate.extraNonce++;
            return make_unique<WorkEthash>(workTemplate);
        }

        ~EthashStratumJob() override = default;

        explicit EthashStratumJob(const std::weak_ptr<Pool> &pool, std::string id)
                : PoolJob(pool)
                , jobId(std::move(id))
                , workTemplate(uniqueNonce) {
        }

    private:
        static const uint32_t uniqueNonce;
    };


    class PoolEthashStratum : public Pool {
    public:
        explicit PoolEthashStratum(const PoolConstructionArgs &);
        ~PoolEthashStratum() override;

    private:
        WorkQueue queue;

        void onDeclaredDead() override;
        void tryConnect();

        // Pool interface
        bool isExpiredJob(const PoolJob &job) override;
        unique_ptr <Work> tryGetWorkImpl() override;
        void submitSolutionImpl(unique_ptr<WorkSolution> resultBase) override;

        void onConnected(CxnHandle);
        jrpc::JsonRpcUtil io {IOMode::Tcp};

        CxnHandle _cxn; //modified only on IO thread
        bool acceptMiningNotify = false; //modified only on IO thread

        void onMiningNotify (const nl::json &j);
    };

}