
#include "AlgoEthashCL.h"
#include <src/compute/ComputeModule.h>
#include <src/pool/WorkEthash.h>
#include <src/util/Logging.h>
#include <src/util/HexString.h> //for debug logging hex
#include <src/common/Endian.h>
#include <src/compute/opencl/CLProgramLoader.h>
#include "Ethash.h"

namespace miner {

    AlgoEthashCL::AlgoEthashCL(AlgoConstructionArgs args)
            : pool(args.workProvider)
            , clProgramLoader(args.compute.getProgramLoaderOpenCL()) {

        LOG(INFO) << "launching " << args.assignedDevices.size() << " gpu-tasks";

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

        auto notifyFunc = [] (const char *errinfo, const void *private_info, size_t cb, void *user_data) {
            LOG(ERROR) << errinfo;
        };

        PerPlatform plat;
        plat.clContext = cl::Context(clDevice, nullptr, notifyFunc); //TODO: opencl context should be per-platform

        auto maybeProgram = clProgramLoader.loadProgram(plat.clContext, "ethash.cl", "-D WORKSIZE=4"); //TODO: build worksize into this string programmatically
        if (!maybeProgram) {
            LOG(ERROR) << "unable to load ethash kernel, aborting algorithm";
            return;
        }
        plat.clProgram = maybeProgram.value();

        DagFile dag;

        while (!shutdown) {
            //get work only for obtaining dag creation info
            auto work = pool.tryGetWork<kEthash>().value_or(nullptr);
            if (!work)
                continue; //check shutdown and try again

            LOG(INFO) << "using work to generate dag";

            //~ every 5 days
            DagCacheContainer *dagCachePtr = nullptr; //TODO: implement read locks and replace this ptr with a read-locked dagCache
            {
                auto cache = dagCache.lock(); //TODO: obtain write-lock here once read/write locks are implemented
                cache->generateIfNeeded(work->epoch, work->seedHash);
                dagCachePtr = &*cache;
            }

            if (!dag.generate(*dagCachePtr, work->seedHash, plat.clContext, clDevice, plat.clProgram)) {
                LOG(ERROR) << "generating dag file failed.";
                continue;
            }

            LOG(INFO) << "launching " << numGpuSubTasks << " gpu-subtasks for the current gpu-task";

            std::vector<std::future<void>> tasks(numGpuSubTasks);

            for (auto &task : tasks) {
                task = std::async(std::launch::async, &AlgoEthashCL::gpuSubTask, this,
                                  std::ref(plat), std::ref(clDevice), std::ref(dag));
            }

            //tasks destructor waits
            //tasks terminate if epoch has changed for every task's work
        }

    }

    void AlgoEthashCL::gpuSubTask(PerPlatform &plat, cl::Device &clDevice, DagFile &dag) {
        const uint32_t intensity = 1024 * 1024;

        cl_int err = 0;

        cl::Device device;
        auto i = device.getInfo<CL_DEVICE_MAX_MEM_ALLOC_SIZE>();

        PerGpuSubTask state;
        state.cmdQueue = cl::CommandQueue(plat.clContext, clDevice, 0, &err);
        if (err || !state.cmdQueue()) {
            LOG(ERROR) << "unable to create command queue in gpuSubTask";
        }

        state.clSearchKernel = cl::Kernel(plat.clProgram, "search", &err);
        if (err) {
            LOG(ERROR) << "unable to create 'search' kernel object from cl program";
        }

        size_t headerSize = 32;
        state.CLbuffer0 = cl::Buffer(plat.clContext, CL_MEM_READ_ONLY | CL_MEM_HOST_WRITE_ONLY,
                headerSize, nullptr, &err);
        if (err) {
            LOG(ERROR) << "unable to allocate opencl header buffer";
            return;
        }

        state.clOutputBuffer = cl::Buffer(plat.clContext, CL_MEM_READ_WRITE, 0x100 * sizeof(cl_uint), nullptr, &err);
        if (err) {
            LOG(ERROR) << "unable to allocate opencl output buffer";
            return;
        }

        while (!shutdown) {
            std::shared_ptr<const Work<kEthash>> work = pool.tryGetWork<kEthash>().value_or(nullptr);
            if (!work)
                continue; //check shutdown and try again

            if (work->epoch != dagCache.lock()->getEpoch()) {//TODO: replace
                LOG(INFO) << "dagcache epoch (" << dagCache.lock()->getEpoch() << ") doesn't match work epoch " << work->epoch;
                break;
            }

            //if (work->epoch != dag.getEpoch()) { //TODO: debug reenable once dagfile exists
            //    break; //terminate task
            //}

            for (uint64_t nonce = 0; nonce < UINT32_MAX && !shutdown; nonce += intensity) {

                uint64_t shiftedExtraNonce = uint64_t(work->extraNonce) << 32ULL;

                uint64_t nonceBegin = nonce | shiftedExtraNonce;
                uint64_t nonceEnd = nonceBegin + intensity;

                auto resultNonces = runKernel(state, dag, *work, nonceBegin, nonceEnd);

                if (!resultNonces.empty()) {
                    //todo: submitTasks lock can be avoided by declaring one vector per subTask inside the gpuTask
                    submitTasks.lock()->push_back(std::async(std::launch::async,
                            &AlgoEthashCL::submitShareTask, this, work, std::move(resultNonces)));
                }

                if (work->expired()) {
                    LOG(INFO) << "aborting kernel loop because work has expired on " << std::this_thread::get_id();
                    break; //get new work
                }
            }
        }
    }

    void AlgoEthashCL::submitShareTask(std::shared_ptr<const Work<kEthash>> work, std::vector<uint32_t> resultNonces) {

        for (auto nonce32 : resultNonces) {//for each possible solution

            uint64_t shiftedExtraNonce = uint64_t(work->extraNonce) << 32ULL;
            uint64_t nonce = nonce32 | shiftedExtraNonce;

            auto result = work->makeWorkResult<kEthash>();

            //calculate proof of work hash from nonce and dag-caches
            EthashRegenhashResult hashes {};
            {
                auto caches = dagCache.lock(); //todo: implement readlock
                hashes = ethash_regenhash(*work, caches->getCache(), nonce);
            }

            result->nonce = nonce;
            result->proofOfWorkHash = hashes.proofOfWorkHash;
            result->mixHash = hashes.mixHash;

            bool isValidSolution = lessThanLittleEndian(result->proofOfWorkHash, work->target);

            if (isValidSolution)
                pool.submitWork(std::move(result));
            else {
                LOG(INFO) << "discarding invalid solution nonce: 0x" << HexString(toBytesWithBigEndian(nonce)).str();
            }
        }
    }

/*
    //C implementation
    std::vector<uint32_t> AlgoEthashCL::runKernel(PerGpuSubTask &state, DagFile &dag, const Work<kEthash> &work,
                                                      uint64_t nonceBegin, uint64_t nonceEnd) {

        std::vector<uint32_t> results;

        auto &cache = *dagCache.lock(); //(bad!) stealing the actual dagCache reference without holding the lock (TODO: remove by implementing read lock)

        LOG(INFO) << "starting nonce range [" << nonceBegin << ", " << nonceEnd << ") on thread " << std::this_thread::get_id();
        for (auto nonce = nonceBegin; nonce < nonceEnd; ++nonce) {
            auto hashes = ethash_regenhash(work, cache.getCache(), nonce);

            if (lessThanLittleEndian(hashes.proofOfWorkHash, work.target)) {
                LOG(INFO) << "result found: nonce: 0x" << HexString(toBytesWithBigEndian(nonce)).str();
                results.push_back(static_cast<uint32_t>(nonce));
            }
        }
        LOG(INFO) << (nonceEnd - nonceBegin) << " nonces traversed on " << std::this_thread::get_id();


        return results;
    }
    */

    std::vector<uint32_t> AlgoEthashCL::runKernel(PerGpuSubTask &state, DagFile &dag, const Work<kEthash> &work,
                                                          uint64_t nonceBegin, uint64_t nonceEnd) {
        cl_int err = 0;
        std::vector<uint32_t> results;

        // Not nodes now (64 bytes), but DAG entries (128 bytes)
        cl_uint ItemsArg = static_cast<cl_uint>(dag.getSize() / 128);
        cl_uint max = static_cast<cl_uint>((cl_ulong) UINT32_MAX * ItemsArg >> 32);
        cl_uint red_shift = UINT32_MAX;
        for (; max != 0; max >>= 1, red_shift++);
        cl_uint red_mult = static_cast<cl_uint>(((cl_ulong) 1 << (32 + red_shift)) / ItemsArg);

        err = state.cmdQueue.enqueueWriteBuffer(state.CLbuffer0, CL_FALSE, 0, work.header.size(), work.header.data());
        if (err) {
            LOG(ERROR) << "#" << err << " error when writing work header to cl buffer";
            return results;
        }

        cl_uint i = 0;
        auto &k = state.clSearchKernel;

        LOG(INFO) << "clOutputBuffer " << state.clOutputBuffer.getInfo<CL_MEM_SIZE>();

        k.setArg(i++, state.clOutputBuffer);
        k.setArg(i++, state.CLbuffer0);
        k.setArg(i++, dag.getCLBuffer());
        k.setArg(i++, nonceBegin);
        k.setArg(i++, work.target.size() - 24, work.target.data() + 24);
        k.setArg(i++, ItemsArg);
        k.setArg(i++, red_mult);
        k.setArg(i++, red_shift);

        cl::NDRange offset{0};
        //TODO: intensity must influence the following work sizes
        cl::NDRange localWorkSize{4}; //worksize from sgminer
        cl::NDRange globalWorkSize{1024}; //rawIntensity (= 2^intensity)

        err = state.cmdQueue.enqueueNDRangeKernel(state.clSearchKernel, offset, globalWorkSize, localWorkSize);
        if (err) {
            LOG(ERROR) << "#" << err << " error (A) when enqueueing search kernel";
        }

        err = state.cmdQueue.finish(); //clFinish();
        if (err) {
            LOG(ERROR) << "#" << err << " error when trying to finish search kernel";
            return results;
        }

        cl_bool blocking = CL_TRUE;
        err = state.cmdQueue.enqueueReadBuffer(state.clOutputBuffer, blocking, 0, state.outputBuffer.size(), state.outputBuffer.data());
        if (err) {
            LOG(ERROR) << "#" << err << " error when trying to read back clOutputBuffer after search kernel";
            return results;
        }

        //TODO: test whether blocking = CL_TRUE works as intended here
        //TODO: alternatively for nvidia, use glFlush instead of finish and wait for the cl event of readBuffer command to finish

        auto numFoundNonces = state.outputBuffer.back(); //last entry is amount of found nonces

        if (numFoundNonces >= state.outputBuffer.size()) {
            LOG(ERROR) << "amount of nonces in outputBuffer exceeds outputBuffer's size";
        }
        else if (numFoundNonces > 0) {
            //clear the clOutputBuffer by overwriting it with blank
            state.cmdQueue.enqueueWriteBuffer(state.clOutputBuffer, CL_FALSE, 0, state.blankOutputBuffer.size(), state.blankOutputBuffer.data());

            //put the nonces into the results vector so they can be moved to a submit task
            auto begin = state.outputBuffer.begin();
            auto end = begin + numFoundNonces;
            results.insert(results.begin(), begin, end);
        }

        LOG(INFO) << "kernel finished, " << results.size() << " results on " << std::this_thread::get_id();
        return results;
    }

}