//
//

#include "Registry.h"

#include <src/algorithm/ethash/AlgoEthashCL.h>
#include <src/algorithm/grin/AlgoCuckatoo31Cl.h>
#include <src/algorithm/dummy/AlgoDummy.h>

#include <src/pool/PoolEthash.h>
#include <src/pool/PoolGrin.h>

#include <src/gpu_api/AmdgpuApi.h>

namespace riner {

    //add new AlgoImpls/PoolImpls or GpuApis in the member functions below

    void Registry::registerAllAlgoImpls() {
        //         <AlgoImpl class>(ConfigName, PowType)
        addAlgoImpl<AlgoEthashCL>("EthashCL", "ethash");
        addAlgoImpl<AlgoCuckatoo31Cl>("Cuckatoo31Cl", "cuckatoo31");
        addAlgoImpl<AlgoDummy>("AlgoDummy", "ethash");
    }

    void Registry::registerAllPoolImpls() {
        //         <PoolImpl class>(ConfigName, PowType)
        addPoolImpl<PoolEthashStratum>("EthashStratum2", "ethash", "stratum2");
        // TODO: make it possible to assign multiple pow_types to one PoolImpl
        addPoolImpl<PoolGrinStratum>("Cuckatoo31Stratum", "cuckatoo31", "stratum");
        // addPoolImpl<PoolGrinStratum>("Cuckaroo29Stratum", "cuckaroo29", "stratum");
    }

    void Registry::registerAllGpuApis() {
        //    <GpuApi class>(SettingsName)
        addGpuApi<AmdgpuApi>("AmdgpuApi");
    }

    //see RegistryImpl.cpp for other member implementations of Registry
}
