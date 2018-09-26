
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

    optional<cl::Device> ComputeModule::getDeviceOpenCL(const DeviceId &requestId) {

        std::vector<cl::Platform> clPlatforms;
        cl::Platform::get(&clPlatforms);

        for (auto &clPlatform : clPlatforms) {
            std::vector<cl::Device> clDevices;
            clPlatform.getDevices(CL_DEVICE_TYPE_ALL, &clDevices);

            for (auto &clDevice : clDevices) {
                if (auto deviceId = obtainDeviceIdFromOpenCLDevice(clDevice)) {
                    if (deviceId == requestId)
                        return clDevice;
                }
            }
        }

        LOG(INFO) << "getDeviceOpenCL: device '" << gsl::to_string(requestId.getName())
        << "' does not have a corresponding opencl device";
        return nullopt;
    }

}