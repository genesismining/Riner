

#pragma once

#include <src/compute/ComputeModule.h>
#include <src/util/Copy.h>
#include <src/util/ConfigUtils.h>
#include <src/pool/Pool.h>

#include <vector>

namespace miner {

    struct AlgoConstructionArgs {
        ComputeModule &compute;
        std::vector<DeviceAlgoInfo> assignedDevices;
        WorkProvider &workProvider;
    };

    class AlgoBase {
    public:
        AlgoBase() {}
        DELETE_COPY_AND_ASSIGNMENT(AlgoBase);

        virtual ~AlgoBase() = default;
    };

}
