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
#include "PoolWithWorkQueue.h"

namespace miner {

    class PoolDummy : public PoolWithWorkQueue {
    public:
        explicit PoolDummy(PoolConstructionArgs);
        ~PoolDummy() override;

    private:
        PoolConstructionArgs args;

        // Pool interface
        void submitSolutionImpl(unique_ptr<WorkSolution>) override;

        jrpc::JsonRpcUtil io; //io utility object
        CxnHandle _cxn; //io connection to pool

        void onConnected(CxnHandle cxn);

        bool acceptWorkMessages = false;

        void onMiningNotify (const nl::json &j);
    };

}