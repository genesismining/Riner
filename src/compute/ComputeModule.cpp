
#include "ComputeModule.h"
#include <src/common/OpenCL.h>
#include <src/common/Optional.h>
#include <src/util/Logging.h>

namespace miner {

    ComputeModule::ComputeModule()
    : allDevices(gatherAllDeviceIds()) {
    }

    span<DeviceId> ComputeModule::getAllDeviceIds() {
        return allDevices;
    }

}