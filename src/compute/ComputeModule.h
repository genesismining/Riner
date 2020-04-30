
#pragma once

#include <src/common/Pointers.h>
#include <src/compute/DeviceId.h>
#include <src/common/Optional.h>
#include <src/common/Span.h>
#include <src/config/Config.h>
#include <vector>

namespace riner {
    class CLProgramLoader;

    class ComputeModule {
        std::vector<DeviceId> allDevices;

        unique_ptr<CLProgramLoader> clProgramLoader; //initialized in ctor

    public:
        /**
         * initialize ComputeModule with a valid protobuf config.
         * param config config required for e.g. OpenCL kernel directory. (this function takes the entire config instead of the kernel directory string because it is forseeable that the ComputeModule will require other config information n the future)
         */
        ComputeModule(const Config &config);
        ~ComputeModule();

        /**
         * return generic (compute api agnostic) `DeviceId`s of every viable compute device that is connected to the machine
         */
        const std::vector<DeviceId> &getAllDeviceIds() const;

        /**
         * obtain the OpenCL specific device handle for a generic DeviceId
         * will print a log message if obtaining the device fails (no logging on user side necessary)
         * return the corresponding `cl::Device` to the given `DeviceId` or nullopt if no OpenCL handle is available for the generic ID (unlikely)
         */
        optional<cl::Device> getDeviceOpenCL(const DeviceId &);
        //optional<vk::Device> getDeviceVulkan(const DeviceId &)

        /**
         * return the ProgramLoader for OpenCL which provides convenience functionality for loading/compiling OpenCL kernels
         * this function is thread safe, more details on the thread safety of `CLProgramLoader` in "CLProgramLoader.h"
         */
        CLProgramLoader &getProgramLoaderOpenCL();
    };

}
