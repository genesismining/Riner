//
//

#include "Types.h"

#include <src/algorithm/ethash/AlgoEthashCL.h>
#include <src/algorithm/grin/AlgoCuckaroo31Cl.h>

#include <src/pool/PoolEthash.h>
#include <src/pool/PoolGrin.h>

namespace miner {

    void registerAlgoTypes(AlgoFactory &f) {

        f.registerType<AlgoEthashCL>("AlgoEthashCL", kEthash);
        f.registerType<AlgoCuckaroo31Cl>("AlgoCuckaroo31Cl", kCuckaroo31);
        //add your new algorithm type here

    }

    void registerPoolTypes(PoolFactory &f) {

        f.registerType<PoolEthashStratum>("PoolEthashStratum", kEthash, kStratumTcp);
        f.registerType<PoolGrinStratum>("PoolGrinStratum", kCuckaroo31, kStratumTcp);

        //add your new pool type here

    }

}
