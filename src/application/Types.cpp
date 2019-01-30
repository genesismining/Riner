//
//

#include "Types.h"
#include <src/algorithm/AlgoFactory.h>
#include <src/algorithm/ethash/AlgoEthashCL.h>
#include <src/pool/PoolFactory.h>
#include <src/pool/PoolEthash.h>

namespace miner {

    void registerAlgoTypes(AlgoFactory &f) {

        f.registerType<AlgoEthashCL>("AlgoEthashCL", kEthash);
        //add your new algorithm type here

    }

    void registerPoolTypes(PoolFactory &f) {

        f.registerType<PoolEthashStratum>("PoolEthashStratum", kEthash, kStratumTcp);
        //add your new pool type here

    }

}