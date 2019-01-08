
#pragma once

#include <future>
#include <atomic>
#include <vector>
#include <src/algorithm/Algorithm.h>
#include <src/pool/Work.h>
#include <src/algorithm/ethash/DagCache.h>
#include <src/algorithm/ethash/DagFile.h>
#include <src/util/LockUtils.h>
#include <src/compute/opencl/CLProgramLoader.h>

#include <mutex>

namespace miner {

    class AlgoEthashCL : AlgoBase {

        LockGuarded<DagCacheContainer> dagCache;
        WorkProvider &pool;
        CLProgramLoader &clProgramLoader;

        std::atomic<bool> shutdown {false};

        struct PerPlatform {
            cl::Context clContext;
            cl::Program clProgram; //ethash.cl
        };

        struct PerGpuSubTask {
            cl::Kernel clSearchKernel;
            cl::CommandQueue cmdQueue; //for clFinish(queue);
            cl::Buffer CLbuffer0;
            cl::Buffer clOutputBuffer;

            constexpr static size_t bufferCount = 0x100;

            std::vector<uint32_t> outputBuffer            = std::vector<uint32_t>(bufferCount, 0); //this is where clOutputBuffer gets read into
            const std::vector<uint32_t> blankOutputBuffer = std::vector<uint32_t>(bufferCount, 0); //used to clear the clOutputBuffer
        };

        //submit tasks are created from several threads, therefore LockGuarded
        LockGuarded<std::vector<std::future<void>>> submitTasks;

        std::vector<std::future<void>> gpuTasks; //one task per gpu

        //gets called once for each gpu
        void gpuTask(cl::Device clDevice);

        //gets called numGpuSubTasks times from each gpuTask
        void gpuSubTask(PerPlatform &, cl::Device &, DagFile &dag);

        //gets called by gpuSubTask for each non-empty result vector
        void submitShareTask(std::shared_ptr<const Work<kEthash>> work, std::vector<uint32_t> resultNonces);

        //returns possible solution nonces
        std::vector<uint32_t> runKernel(PerGpuSubTask &, DagFile &dag, const Work<kEthash> &,
                uint64_t nonceBegin, uint64_t nonceEnd);

    public:
        explicit AlgoEthashCL(AlgoConstructionArgs);

        //destructor shuts down all working threads and joins them
        ~AlgoEthashCL();

    };

}