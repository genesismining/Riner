
#include "PoolEthash.h"

namespace miner {

    unique_ptr<WorkBase> PoolEthashStratum::getWork() {
        return nullptr;
    }

    void PoolEthashStratum::submitWork(unique_ptr<WorkResultBase> result) {

    }

}