
#include "AlgoEthashCL.h"
#include <src/compute/ComputeModule.h>
#include <src/pool/WorkEthash.h>
#include <src/util/Logging.h>
#include <src/util/HexString.h> //for debug logging hex
#include <src/common/Endian.h>
#include <src/compute/opencl/CLProgramLoader.h>
#include <src/common/Future.h>
#include <memory>

namespace miner {

    AlgoEthashCL::AlgoEthashCL(AlgoConstructionArgs args)
            : pool(args.workProvider)
            , clProgramLoader(args.compute.getProgramLoaderOpenCL()) {
        LOG(INFO) << "launching " << args.assignedDevices.size() << " gpu-tasks";

        for (auto &device : args.assignedDevices) {
            if (auto clDevice = args.compute.getDeviceOpenCL(device.get().id)) {

                gpuTasks.push_back(std::async(std::launch::async, &AlgoEthashCL::gpuTask, this,
                                              std::move(*clDevice), device));
            }
        }
    }

    AlgoEthashCL::~AlgoEthashCL() {
        shutdown = true; //set atomic shutdown flag
        //implicitly waits for gpuTasks and submitTasks to finish
    }

    void AlgoEthashCL::gpuTask(cl::Device clDevice, Device &device) {
        const auto &settings = device.settings;
        const unsigned numGpuSubTasks = settings.num_threads;

        //Statistics statistics;

        auto notifyFunc = [] (const char *errinfo, const void *private_info, size_t cb, void *user_data) {
            LOG(ERROR) << errinfo;
        };

        PerPlatform plat;
        plat.clContext = cl::Context(clDevice, nullptr, notifyFunc); //TODO: opencl context should be per-platform

        std::string compilerOptions = "-D WORKSIZE=" + std::to_string(settings.work_size);

        auto maybeProgram = clProgramLoader.loadProgram(plat.clContext, "/kernel/ethash.cl", compilerOptions);
        if (!maybeProgram) {
            LOG(ERROR) << "unable to load ethash kernel, aborting algorithm";
            return;
        }
        plat.clProgram = *maybeProgram;

        DagFile dag;

        while (!shutdown) {
            //get work only for obtaining dag creation info
            LOG(INFO) << "trying to get work for dag creation";
            auto work = pool.tryGetWork<WorkEthash>();
            if (!work)
                continue; //check shutdown and try again

            LOG(INFO) << "using work to generate dag";

            //~ every 5 days
            {
                LOG(INFO) << "waiting for dag cache to be generated";
                typedef decltype(dagCache.readLock()) read_locked_cache_t;
                unique_ptr<read_locked_cache_t> readCachePtr;
                auto lockedCache = dagCache.immediateLock();
                if (!lockedCache->isGenerated(work->epoch)) {
                    auto writeCache = lockedCache.upgrade();
                    writeCache->generate(work->epoch, work->seedHash);
                    readCachePtr = std::make_unique<read_locked_cache_t>(writeCache.downgrade());
                }
                else {
                    readCachePtr = std::make_unique<read_locked_cache_t>(lockedCache.downgrade());
                }
                auto &readCache = *readCachePtr;
                LOG(INFO) << "dag cache was generated for epoch " << readCache->getEpoch();

                if (!dag.generate(*readCache, plat.clContext, clDevice, plat.clProgram)) {
                    LOG(ERROR) << "generating dag file failed.";
                    continue;
                }
            }

            LOG(INFO) << "launching " << numGpuSubTasks << " gpu-subtasks for the current gpu-task";

            std::vector<std::future<void>> tasks(numGpuSubTasks);

            for (auto &task : tasks) {
                task = std::async(std::launch::async, &AlgoEthashCL::gpuSubTask, this,
                                  std::ref(plat), std::ref(clDevice), std::ref(dag), std::ref(device));
            }

            //tasks destructor waits
            //tasks terminate if epoch has changed for every task's work
        }

    }

    void AlgoEthashCL::gpuSubTask(PerPlatform &plat, cl::Device &clDevice, DagFile &dag, Device &device) {
        cl_int err = 0;

        PerGpuSubTask state;
        state.settings = device.settings;

        state.cmdQueue = cl::CommandQueue(plat.clContext, clDevice, 0, &err);
        if (err || !state.cmdQueue()) {
            LOG(ERROR) << "unable to create command queue in gpuSubTask";
        }

        state.clSearchKernel = cl::Kernel(plat.clProgram, "search", &err);
        if (err) {
            LOG(ERROR) << "unable to create 'search' kernel object from cl program";
        }

        size_t headerSize = 32;
        state.header = cl::Buffer(plat.clContext, CL_MEM_READ_ONLY | CL_MEM_HOST_WRITE_ONLY,
                headerSize, nullptr, &err);
        if (err) {
            LOG(ERROR) << "unable to allocate opencl header buffer";
            return;
        }

        state.clOutputBuffer = cl::Buffer(plat.clContext, CL_MEM_READ_WRITE, state.bufferSize, nullptr, &err);
        if (err) {
            LOG(ERROR) << "unable to allocate opencl output buffer";
            return;
        }
        err = state.cmdQueue.enqueueFillBuffer(state.clOutputBuffer, (uint8_t)0, 0, state.bufferSize);
        if (err) {
            LOG(ERROR) << "unable to clear opencl output buffer";
            return;
        }

        while (!shutdown) {
            std::shared_ptr<const WorkEthash> work = pool.tryGetWork<WorkEthash>();
            if (!work)
                continue; //check shutdown and try again

            if (work->epoch != dag.getEpoch()) {
                break; //terminate task
            }

            auto raw_intensity = state.settings.raw_intensity;

            for (uint64_t nonce = 0; nonce < UINT32_MAX && !shutdown; nonce += raw_intensity) {

                uint64_t shiftedExtraNonce = uint64_t(work->extraNonce) << 32ULL;

                uint64_t nonceBegin = nonce | shiftedExtraNonce;
                uint64_t nonceEnd = nonceBegin + raw_intensity;

                auto resultNonces = runKernel(state, dag, *work, nonceBegin, nonceEnd);

                for (uint64_t resultNonce : resultNonces) {
                    tasks.addTask(std::bind(&AlgoEthashCL::submitShare, this, work, resultNonce, std::ref(device)));
                }
                device.records.reportScannedNoncesAmount(raw_intensity);

                if (work->expired()) {
                    LOG(INFO) << "aborting kernel loop because work has expired on " << std::this_thread::get_id();
                    break; //get new work
                }
            }
        }
    }

    void AlgoEthashCL::submitShare(std::shared_ptr<const WorkEthash> work, uint64_t nonce, Device &device) {

        auto result = work->makeWorkSolution<WorkSolutionEthash>();

        //calculate proof of work hash from nonce and dag-caches
        auto hashes = dagCache.readLock()->getHash(work->header, nonce);

        result->nonce = nonce;
        result->header = work->header;
        result->mixHash = hashes.mixHash;

        if (lessThanLittleEndian(hashes.proofOfWorkHash, work->jobTarget))
            pool.submitSolution(std::move(result));

        bool isValidSolution = lessThanLittleEndian(hashes.proofOfWorkHash, work->deviceTarget);
        device.records.reportWorkUnit(work->deviceDifficulty, isValidSolution);
        if (!isValidSolution) {
            LOG(INFO) << "discarding invalid solution nonce: 0x" << HexString(toBytesWithBigEndian(nonce)).str();
        }
    }

    std::vector<uint64_t> AlgoEthashCL::runKernel(PerGpuSubTask &state, DagFile &dag, const WorkEthash &work,
                                                          uint64_t nonceBegin, uint64_t nonceEnd) {
        cl_int err = 0;
        std::vector<uint64_t> results;

        cl_uint size = dag.getSize();
        cl_uint isolate = UINT32_MAX;
        uint64_t target64 = 0;
        MI_EXPECTS(work.deviceTarget.size() - 24 == sizeof(target64));
        memcpy(&target64, work.deviceTarget.data() + 24, work.deviceTarget.size() - 24);

        err = state.cmdQueue.enqueueWriteBuffer(state.header, CL_FALSE, 0, work.header.size(), work.header.data());
        if (err) {
            LOG(ERROR) << "#" << err << " error when writing work header to cl buffer";
            return results;
        }

        cl_uint argI = 0;
        auto &k = state.clSearchKernel;

        k.setArg(argI++, state.clOutputBuffer);
        k.setArg(argI++, state.header);
        k.setArg(argI++, dag.getCLBuffer());
        k.setArg(argI++, size);
        k.setArg(argI++, nonceBegin);
        k.setArg(argI++, target64);
        k.setArg(argI++, isolate);

        cl::NDRange offset{0};

        cl::NDRange localWorkSize{state.settings.work_size};
        cl::NDRange globalWorkSize{state.settings.raw_intensity};

        err = state.cmdQueue.enqueueNDRangeKernel(state.clSearchKernel, offset, globalWorkSize, localWorkSize);
        if (err) {
            LOG(ERROR) << "#" << err << " error when enqueueing search kernel on " << std::this_thread::get_id();
        }

        cl_bool blocking = CL_FALSE;
        err = state.cmdQueue.enqueueReadBuffer(state.clOutputBuffer, blocking, 0, state.bufferSize, state.outputBuffer.data());
        if (err) {
            LOG(ERROR) << "#" << err << " error when trying to read back clOutputBuffer after search kernel";
            return results;
        }
        state.cmdQueue.finish();
        //TODO: alternatively for nvidia, use clFlush instead of finish and wait for the cl event of readBuffer command to finish

        auto numFoundNonces = state.outputBuffer.back();
        if (numFoundNonces >= state.bufferCount) {
            LOG(ERROR) << "amount of nonces (" << numFoundNonces << ") in outputBuffer exceeds outputBuffer's size";
        }
        else if (numFoundNonces > 0) {
            //clear the clOutputBuffer by overwriting it with blank
            state.cmdQueue.enqueueFillBuffer(state.clOutputBuffer, (uint8_t)0, state.bufferSize - state.bufferEntrySize, state.bufferEntrySize);

            //populate the result vector and compute the actual 32 bit nonces by adding the outputBuffer contents
            results.resize(numFoundNonces);
            for (size_t i = 0; i < numFoundNonces; ++i) {
                results[i] = nonceBegin + state.outputBuffer[i];
            }
        }

        MI_EXPECTS(results.size() == numFoundNonces);
        return results;
    }

}
