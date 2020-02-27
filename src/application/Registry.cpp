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
        addAlgoImpl<AlgoEthashCL>("AlgoEthashCL", "ethash");
        addAlgoImpl<AlgoCuckatoo31Cl>("AlgoCuckatoo31Cl", "cuckatoo31");
        addAlgoImpl<AlgoDummy>("AlgoDummy", "ethash");
    }

    void Registry::registerAllPoolImpls() {
        //         <PoolImpl class>(ConfigName, PowType)
        addPoolImpl<PoolEthashStratum>("PoolEthashStratum", "ethash", "stratum2");
        addPoolImpl<PoolGrinStratum>("PoolGrin", "grin", "GrinStratum", "stratum");
    }

    void Registry::registerAllGpuApis() {
        //    <GpuApi class>(SettingsName)
        addGpuApi<AmdgpuApi>("AmdgpuApi");
    }

    //see RegistryImpl.cpp for other member implementations of Registry
}