
#pragma once

namespace miner {
    class AlgoFactory;
    class PoolFactory;

    void registerAlgoTypes(AlgoFactory &);
    void registerPoolTypes(PoolFactory &);

}