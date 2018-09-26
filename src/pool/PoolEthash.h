
#pragma once

#include <src/pool/Pool.h>
#include <src/pool/WorkEthash.h>
#include <src/network/TcpJsonSubscription.h>

namespace miner {

    class PoolEthashStratum : public WorkProvider {
    protected:
        unique_ptr<WorkBase> getWork() override;

        void submitWork(unique_ptr<WorkResultBase> result) override;

    private:
        TcpJsonSubscription tcpJson;
    };

}