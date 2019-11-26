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
        unique_ptr <Work> tryGetWorkImpl() override;
        void submitSolutionImpl(unique_ptr<WorkSolution>) override;

        jrpc::JsonRpcUtil io; //io utility object
        CxnHandle _cxn; //io connection to pool

        void onConnected(CxnHandle cxn);

        bool acceptWorkMessages = false;

        void onMiningNotify (const nl::json &j);
    };

}