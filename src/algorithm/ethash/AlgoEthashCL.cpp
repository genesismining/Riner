
#include "AlgoEthashCL.h"
#include <src/compute/ComputeModule.h>
#include <src/pool/WorkEthash.h>

namespace miner {

    AlgoEthashCL::AlgoEthashCL(AlgoConstructionArgs args)
            : pool(args.workProvider) {

        for (auto &id : args.assignedDevices) {
            if (auto clDevice = args.compute.getDeviceOpenCL(id)) {
                gpuTasks.push_back(std::async(std::launch::async, &AlgoEthashCL::gpuTask, this,
                                              clDevice.value()));
            }
        }
    }

    AlgoEthashCL::~AlgoEthashCL() {
        shutdown = true; //set atomic shutdown flag
        //implicitly waits for gpuTasks and submitTask to finish
    }

    void AlgoEthashCL::gpuTask(cl::Device clDevice) {

        const unsigned numGpuSubTasks = 8;

        DagFile dag;

        while (!shutdown) {
            auto work = pool.getWork<kEthash>(); //only for obtaining dag creation info
            dag.generate(work->epoch, work->seedHash, clDevice); //~ every 5 days

            std::vector<std::future<void>> tasks(numGpuSubTasks);

            for (auto &task : tasks) {

                task = std::async(std::launch::async, &AlgoEthashCL::gpuSubTask, this,
                        clDevice, std::ref(dag));
            }
            //tasks destructor waits
            //tasks terminate if epoch has changed for every task's work
        }

    }

    void AlgoEthashCL::gpuSubTask(cl::Device clDevice, DagFile &dag) {

        const uint32_t intensity = 24;

        while (!shutdown) {
            auto work = pool.getWork<kEthash>();

            if (work->epoch != dag.getEpoch())
                break; //terminate task

            for (uint64_t nonce = 0; nonce < UINT32_MAX; nonce += intensity) {

                uint64_t shiftedExtraNonce = uint64_t(work->extraNonce) << 32ULL;

                uint64_t nonceBegin = nonce | shiftedExtraNonce;
                uint64_t nonceEnd = nonceBegin + intensity;

                auto results = runKernel(clDevice, *work, nonceBegin, nonceEnd);

                for (auto &result : results) {
                    submitTask = std::async(std::launch::async, &AlgoEthashCL::submitShareTask, this,
                            std::move(result), std::move(submitTask));
                }
            }
        }

    }

    void AlgoEthashCL::submitShareTask(unique_ptr<WorkResult<kEthash>> result, std::future<void> &&previousTask) {
        previousTask.wait(); //wait for the previous submit task to be finished

        if (true) { //result.verify(dagCaches)) {
            pool.submitWork(std::move(result));
        }
    }

    std::vector<unique_ptr<WorkResult<kEthash>>> AlgoEthashCL::runKernel(cl::Device device, const Work<kEthash> &work,
                                uint64_t nonceBegin, uint64_t nonceEnd) {

        std::vector<unique_ptr<WorkResult<kEthash>>> results;
        results.push_back(work.makeWorkResult<kEthash>());
        return results;
    }
}