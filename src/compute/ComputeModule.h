
#pragma once

#include <src/common/Pointers.h>
#include <src/compute/DeviceId.h>
#include <src/common/Optional.h>
#include <src/common/Span.h>
#include <src/config/Config.h>
#include <vector>

namespace miner {
    class CLProgramLoader;

    class ComputeModule {
        std::vector<DeviceId> allDevices;

        unique_ptr<CLProgramLoader> clProgramLoader;

    public:
        ComputeModule(const Config &config);
        ~ComputeModule();

        const std::vector<DeviceId> &getAllDeviceIds() const;

        optional<cl::Device> getDeviceOpenCL(const DeviceId &);
        //optional<vk::Device> getDeviceVulkan(const DeviceId &)

        CLProgramLoader &getProgramLoaderOpenCL();
    };

}
