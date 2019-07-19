
#pragma once

#include <future>
#include <atomic>
#include <vector>
#include <src/algorithm/Algorithm.h>
#include <src/pool/Pool.h>
#include <src/algorithm/ethash/DagCache.h>
#include <src/algorithm/ethash/DagFile.h>
#include <src/util/LockUtils.h>
#include <src/compute/opencl/CLProgramLoader.h>

#include <mutex>

namespace miner {

    class AlgoEthashCL : public Algorithm {

        UpgradeableLockGuarded<DagCacheContainer> dagCache;
        Pool &pool;
        CLProgramLoader &clProgramLoader;

        std::atomic<bool> shutdown {false};

        struct PerPlatform {
            cl::Context clContext;
            cl::Program clProgram; //ethash.cl
        };

        struct PerGpuSubTask {
            cl::Kernel clSearchKernel;
            cl::CommandQueue cmdQueue; //for clFinish(queue);
            cl::Buffer header;
            cl::Buffer clOutputBuffer;

            //Statistics &statistics;

            Device::AlgoSettings settings;

            typedef uint32_t buffer_entry_t;
            constexpr static size_t bufferCount = 0x100;
            constexpr static size_t bufferEntrySize = sizeof(buffer_entry_t);
            constexpr static size_t bufferSize = bufferCount * bufferEntrySize;
            std::vector<buffer_entry_t> outputBuffer = std::vector<buffer_entry_t>(bufferCount, 0); //this is where clOutputBuffer gets read into
        };

        //submit tasks are created from several threads, therefore LockGuarded
        LockGuarded<std::future<void>> submitTask;

        std::vector<std::future<void>> gpuTasks; //one task per gpu

        //gets called once for each gpu
        void gpuTask(cl::Device clDevice, Device::AlgoSettings deviceSettings);

        //gets called numGpuSubTasks times from each gpuTask
        void gpuSubTask(PerPlatform &, cl::Device &, DagFile &dag, Device::AlgoSettings deviceSettings);

        //gets called by gpuSubTask for each non-empty result vector
        void submitShareTask(std::shared_ptr<const WorkEthash> work, std::vector<uint64_t> resultNonces);

        //returns possible solution nonces
        std::vector<uint64_t> runKernel(PerGpuSubTask &, DagFile &dag, const WorkEthash &,
                uint64_t nonceBegin, uint64_t nonceEnd);

    public:
        explicit AlgoEthashCL(AlgoConstructionArgs);

        //destructor shuts down all working threads and joins them
        ~AlgoEthashCL() override;

    };

}
