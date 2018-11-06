
#include "AlgoEthashCL.h"
#include <src/compute/ComputeModule.h>
#include <src/pool/WorkEthash.h>
#include <src/util/Logging.h>
#include <src/common/Future.h>
#include <random>
#include <src/util/HexString.h> //for debug logging hex

namespace miner {

    AlgoEthashCL::AlgoEthashCL(AlgoConstructionArgs args)
            : pool(args.workProvider) {

        for (auto &id : args.assignedDevices) {
            if (auto clDevice = args.compute.getDeviceOpenCL(id)) {
                gpuTasks.push_back(std::async(std::launch::async, &AlgoEthashCL::gpuTask, this,
                                              std::move(clDevice.value())));
            }
        }
    }

    AlgoEthashCL::~AlgoEthashCL() {
        shutdown = true; //set atomic shutdown flag
        //implicitly waits for gpuTasks and submitTasks to finish
    }

    void AlgoEthashCL::gpuTask(cl::Device clDevice) {

        const unsigned numGpuSubTasks = 4;

        DagFile dag;

        while (!shutdown) {
            //get work only for obtaining dag creation info
            auto work = pool.tryGetWork<kEthash>().value_or(nullptr);
            if (!work)
                continue; //check shutdown and try again

            LOG(INFO) << "use work to generate dag...";

            dag.generate(work->epoch, work->seedHash, clDevice); //~ every 5 days

            std::vector<std::future<void>> tasks(numGpuSubTasks);

            for (auto &task : tasks) {

                task = std::async(std::launch::async, &AlgoEthashCL::gpuSubTask, this,
                                  std::ref(clDevice), std::ref(dag));
            }
            //tasks destructor waits
            //tasks terminate if epoch has changed for every task's work
        }

    }

    void AlgoEthashCL::gpuSubTask(const cl::Device &clDevice, DagFile &dag) {
        const uint32_t intensity = 24 * 1000;

        while (!shutdown) {
            auto work = pool.tryGetWork<kEthash>().value_or(nullptr);
            if (!work)
                continue; //check shutdown and try again

            //LOG(INFO) << "gpu thread got ethash work";

            if (work->epoch != dag.getEpoch()) {
                break; //terminate task
            }

            for (uint64_t nonce = 0; nonce < UINT32_MAX && !shutdown; nonce += intensity) {

                uint64_t shiftedExtraNonce = uint64_t(work->extraNonce) << 32ULL;

                uint64_t nonceBegin = nonce | shiftedExtraNonce;
                uint64_t nonceEnd = nonceBegin + intensity;

                auto results = runKernel(clDevice, *work, nonceBegin, nonceEnd);

                if (!results.empty()) {
                    auto submitGuard = submitTasks.lock();
                    for (auto &result : results)
                        submitGuard->push_back(std::async(std::launch::async, &AlgoEthashCL::submitShareTask, this,
                                                          std::move(result)));
                    //submitGuard unlocks
                }

                if (work->expired()) {
                    break; //get new work
                }
            }
        }
    }

    void AlgoEthashCL::submitShareTask(unique_ptr<WorkResult<kEthash>> result) {
        bool workIsValid = true;
        if (workIsValid) {
            pool.submitWork(std::move(result));
        }
    }

    std::vector<unique_ptr<WorkResult<kEthash>>> AlgoEthashCL::runKernel(const cl::Device &device, const Work<kEthash> &work,
                                uint64_t nonceBegin, uint64_t nonceEnd) {

        std::vector<unique_ptr<WorkResult<kEthash>>> results;

        std::random_device rd;
        std::mt19937 gen(rd());
        std::uniform_int_distribution<> dis(1, UINT32_MAX);

        auto num = dis(gen);

        for (uint32_t div = 4000000; div < 1000000000; div *= 10) {
            if (num < UINT32_MAX / div) {
                results.push_back(work.makeWorkResult<kEthash>());
            }
        }

        return results;
    }
}