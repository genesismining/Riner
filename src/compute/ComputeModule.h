
#pragma once

#include <src/common/Pointers.h>
#include <src/compute/DeviceId.h>
#include <src/common/Span.h>
#include <vector>

namespace miner {
    class OpenCLSession;

    class ComputeModule {
        std::vector<DeviceId> allDevices;

    public:
        ComputeModule();

        span<DeviceId> getAllDeviceIds();
        unique_ptr<OpenCLSession> makeOpenCLSession();
        //unique_ptr<VulkanSession> makeVulkanSession();
    };

}
