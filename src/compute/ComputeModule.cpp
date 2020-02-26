
#include "ComputeModule.h"
#include <src/common/OpenCL.h>
#include <src/common/Optional.h>
#include <src/util/Logging.h>
#include <src/common/Assert.h>
#include <src/compute/opencl/CLProgramLoader.h>
#include <src/config/Config.h>

namespace miner {

    ComputeModule::ComputeModule(const Config &config)
    : allDevices(gatherAllDeviceIds()) {
        VLOG(5) << "compute ctor scope...";
        auto kernelDir = config.global_settings().opencl_kernel_dir();
        auto precompiledDir = kernelDir + "precompiled/";
        VLOG(5) << "CLProgramLoader make_unique...";
        clProgramLoader = std::make_unique<CLProgramLoader>(kernelDir, precompiledDir);
        VLOG(5) << "CLProgramLoader make_unique...done";
        VLOG(5) << "compute ctor scope...done";
    }

    ComputeModule::~ComputeModule() {
        //this empty destructor enables forward declaration of types contained in member unique_ptrs
    }

    const std::vector<DeviceId> &ComputeModule::getAllDeviceIds() const {
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

        LOG(INFO) << "ComputeModule: in 'getDeviceOpenCL' the device '" << requestId.getName()
                  << "' does not have a corresponding opencl device";
        return nullopt;
    }

    CLProgramLoader &ComputeModule::getProgramLoaderOpenCL() {
        MI_ENSURES(clProgramLoader);
        return *clProgramLoader;
    }

}
