
#pragma once

#include <src/algorithm/AlgoFactory.h>
#include <src/pool/PoolFactory.h>


namespace miner {
    void registerAlgoTypes(AlgoFactory &);
    void registerPoolTypes(PoolFactory &);
}
