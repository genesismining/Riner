

#pragma once

#include <src/common/Span.h>

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