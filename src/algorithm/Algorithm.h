

#pragma once

#include <vector>
#include <src/util/ConfigUtils.h>

namespace miner {
    class ComputeModule;
    class WorkProvider;
    class DeviceId;

    struct AlgoConstructionArgs {
        ComputeModule &compute;
        std::vector<std::reference_wrapper<Device>> assignedDevices;
        WorkProvider &workProvider;
    };

    class AlgoBase {
    public:
        virtual ~AlgoBase() = default;
    };

}