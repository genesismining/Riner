
#include "AlgoEthashCL.h"
#include <src/compute/ComputeModule.h>
#include <src/pool/WorkEthash.h>
#include <src/util/Logging.h>
#include <src/common/Future.h>
#include <random>
#include <src/util/HexString.h> //for debug logging hex
#include "Ethash.h"

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

            LOG(INFO) << "using work to generate dag";

            //~ every 5 days
            dagCaches.lock()->generateIfNeeded(work->epoch, work->seedHash);
            dag.generate(work->epoch, work->seedHash, clDevice);

            LOG(INFO) << "launching gpu subtasks";

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
        const uint32_t intensity = 1000;

        while (!shutdown) {
            std::shared_ptr<const Work<kEthash>> work = pool.tryGetWork<kEthash>().value_or(nullptr);
            if (!work)
                continue; //check shutdown and try again

            if (work->epoch != dag.getEpoch()) {
                break; //terminate task
            }

            for (uint64_t nonce = 0; nonce < UINT32_MAX && !shutdown; nonce += intensity) {

                uint64_t shiftedExtraNonce = uint64_t(work->extraNonce) << 32ULL;

                uint64_t nonceBegin = nonce | shiftedExtraNonce;
                uint64_t nonceEnd = nonceBegin + intensity;

                auto resultNonces = runKernel(clDevice, *work, nonceBegin, nonceEnd);

                if (!resultNonces.empty()) {
                    submitTasks.lock()->push_back(std::async(std::launch::async,
                            &AlgoEthashCL::submitShareTask, this, work, std::move(resultNonces)));
                }

                if (work->expired()) {
                    break; //get new work
                }
            }
        }
    }

    void AlgoEthashCL::submitShareTask(std::shared_ptr<const Work<kEthash>> work, std::vector<uint64_t> resultNonces) {

        for (auto nonce : resultNonces) {//for each possible solution

            auto result = work->makeWorkResult<kEthash>();

            //calculate proof of work hash from nonce and dag-caches
            {
                auto caches = dagCaches.lock(); //todo: implement readlock
                auto hashes = ethash_regenhash(*work, caches->getCache(), nonce);

                result->nonce = nonce;
                result->proofOfWorkHash = hashes.proofOfWorkHash;
                result->mixHash = hashes.mixHash;
            }

            //TODO: verify that the byte order of these works with std::array operator<();
            bool isValidSolution = result->proofOfWorkHash < work->target;

            if (isValidSolution)
                pool.submitWork(std::move(result));
            else {
                LOG(INFO) << "discarding invalid solution nonce: 0x" << HexString(nonce).str();
                LOG(WARNING) << "notice: the byte order may be the wrong way for operator<() of std::array to be a correct test against target";
            }
        }
    }

    std::vector<uint64_t> AlgoEthashCL::runKernel(const cl::Device &device, const Work<kEthash> &work,
                                uint64_t nonceBegin, uint64_t nonceEnd) {

        std::vector<uint64_t> results;

        auto &dagCache = *dagCaches.lock(); //(bad!) stealing the actual dagCache reference without holding the lock (TODO: remove)

        LOG(INFO) << "starting nonce range [" << nonceBegin << ", " << nonceEnd << ") on thread " << std::this_thread::get_id();
        for (auto nonce = nonceBegin; nonce < nonceEnd; ++nonce) {
            auto hashes = ethash_regenhash(work, dagCache.getCache(), nonce);
            if (hashes.proofOfWorkHash < work.target)
                results.push_back(nonce);
        }
        LOG(INFO) << (nonceEnd - nonceBegin) << " nonces traversed on " << std::this_thread::get_id();


        return results;
    }
}