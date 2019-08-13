
#pragma once

#include <src/pool/PoolWithWorkQueue.h>
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

    struct EthashStratumJob : public PoolJob {

        const std::string jobId;
        WorkEthash workTemplate;

        std::unique_ptr<Work> makeWork() override {
            workTemplate.setEpoch();
            workTemplate.extraNonce++;
            return std::make_unique<WorkEthash>(workTemplate);
        }

        ~EthashStratumJob() override = default;

        explicit EthashStratumJob(std::string id)
                : jobId(std::move(id))
                , workTemplate(uniqueNonce) {
        }

    private:
        static const uint32_t uniqueNonce;
    };


    class PoolEthashStratum : public PoolWithWorkQueue {
    public:
        explicit PoolEthashStratum(PoolConstructionArgs);
        ~PoolEthashStratum() override;

        cstring_span getName() const override;

    private:
        PoolConstructionArgs args;

        // Pool interface
        void submitSolutionImpl(unique_ptr<WorkSolution> resultBase) override;


        void onConnected(CxnHandle);
        jrpc::JsonRpcUtil io {IOMode::Tcp};

        CxnHandle _cxn; //modified only on IO thread
        bool acceptMiningNotify = false; //modified only on IO thread

        void onMiningNotify (const nl::json &j);
    };

}