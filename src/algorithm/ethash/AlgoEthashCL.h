
#pragma once

#include <future>
#include <atomic>
#include <vector>
#include <src/algorithm/Algorithm.h>
#include <src/pool/Pool.h>
#include <src/algorithm/ethash/DagCache.h>
#include <src/algorithm/ethash/DagFile.h>
#include <src/util/LockUtils.h>
#include <src/util/TaskExecutorPool.h>
#include <src/compute/opencl/CLProgramLoader.h>

#include <mutex>

namespace riner {

    /**
     * AlgoImpl for powType "ethash" with compute Api "OpenCL"
     */
    class AlgoEthashCL : public Algorithm {

        UpgradeableLockGuarded<DagCacheContainer> dagCache;
        Pool &pool;
        CLProgramLoader &clProgramLoader;

        std::atomic<bool> shutdown {false};

        struct PerPlatform { //this struct exists once per opencl platform
            cl::Context clContext;
            cl::Program clProgram; //ethash.cl
        };

        struct PerGpuSubTask { //this struct exists once per gpu subtask (which is usually more than one task per gpu)
            cl::Kernel clSearchKernel;
            cl::CommandQueue cmdQueue; //for clFinish(queue);
            cl::Buffer header;
            cl::Buffer clOutputBuffer;
            AlgoSettings settings;

            typedef uint32_t buffer_entry_t;
            constexpr static size_t bufferCount = 0x100;
            constexpr static size_t bufferEntrySize = sizeof(buffer_entry_t);
            constexpr static size_t bufferSize = bufferCount * bufferEntrySize;
            std::vector<buffer_entry_t> outputBuffer = std::vector<buffer_entry_t>(bufferCount, 0); //this is where clOutputBuffer gets read into
        };

        //used to execute share submission tasks. It's safe to not track the futures returned by addTask() due to the lifetime of tasks
        TaskExecutorPool tasks {std::min(std::thread::hardware_concurrency(), 2U)};

        std::vector<std::future<void>> gpuTasks; //one task per gpu

        //gets called once for each gpu
        void gpuTask(size_t taskIndex, cl::Device clDevice, Device &device);

        //gets called numGpuSubTasks times from each gpuTask
        void gpuSubTask(size_t subTaskIndex, PerPlatform &, cl::Device &, DagFile &dag, Device &deviceSettings);

        //gets called by gpuSubTask for each nonce found
        void submitShare(std::shared_ptr<const WorkEthash> work, uint64_t nonce, Device &device);

        //returns possible solution nonces
        std::vector<uint64_t> runKernel(PerGpuSubTask &, DagFile &dag, const WorkEthash &,
                uint64_t nonceBegin, uint64_t nonceEnd);

    public:
        //algorithm starts working as soon as constructor is called
        explicit AlgoEthashCL(AlgoConstructionArgs);

        //destructor shuts down all working threads and joins them
        ~AlgoEthashCL() override;

    };

}
