//
//

#include "Types.h"

#include <src/algorithm/ethash/AlgoEthashCL.h>
#include <src/algorithm/grin/AlgoCuckatoo31Cl.h>

#include <src/pool/PoolEthash.h>
#include <src/pool/PoolGrin.h>

namespace miner {

    void registerAlgoTypes(AlgoFactory &f) {

        f.registerType<AlgoEthashCL>("AlgoEthashCL", kEthash);
        f.registerType<AlgoCuckatoo31Cl>("AlgoCuckatoo31Cl", kCuckatoo31);
        //add your new algorithm type here

    }

    void registerPoolTypes(PoolFactory &f) {

        f.registerType<PoolEthashStratum>("PoolEthashStratum", kEthash, kStratumTcp);
        f.registerType<PoolGrinStratum>("PoolGrinStratum", kCuckatoo31, kStratumTcp);

        //add your new pool type here

    }

}
