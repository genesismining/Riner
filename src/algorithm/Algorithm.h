

#pragma once

#include <src/common/Span.h>
#include <src/common/Pointers.h>
#include <src/common/OpenCL.h>
#include <atomic>

namespace miner {
    class ComputeModule;
    class WorkProvider;
    class DeviceId;

    struct AlgoConstructionArgs {
        ComputeModule &compute;
        span<DeviceId> assignedDevices;
        WorkProvider &workProvider;
    };

    class AlgoBase {

    };

}