#include <src/algorithm/grin/Cuckaroo.h>
#include <string>
#include <vector>

#include <src/crypto/blake2.h>
#include <src/common/Endian.h>
#include <src/pool/WorkCuckaroo.h>
#include <src/util/Logging.h>

namespace miner {

namespace {
constexpr uint32_t remainingEdges[8] =
{
        1U << 31,
        1357500000,
         636100000,
         376400000,
         250700000,
         180300000,
         135400000,
         105800000,
};


void checkErr(cl_int err) {

}

void fillBuffer(cl::CommandQueue queue, cl::Buffer buffer, uint8_t pattern, size_t bufferOffset, size_t size) {
    // TODO switch based on opencl version
    //#define USE_CL_ENQUEUE_FILL_BUFFER
#ifdef USE_CL_ENQUEUE_FILL_BUFFER
    std::pair<size_t, std::unique_ptr<cl_event[]>> events = dependencies.toArray();
    cl_int err = clEnqueueFillBuffer(queue, buffer.buffer_, &pattern, 1, bufferOffset, size, events.first,
            events.second.get(), nullptr);
    checkErr(err, "fillBuffer " + buffer.getName());
#else
    std::unique_ptr<uint8_t[]> memory(new uint8_t[size]);
    memset(memory.get(), pattern, size);
    queue.enqueueWriteBuffer(buffer, true, bufferOffset, size, memory.get());
#endif
}


}  // namespace

using std::shared_ptr;
using std::unique_ptr;
using std::make_unique;
using std::make_shared;

unique_ptr<CuckooSolution> CuckatooSolver::solve(unique_ptr<CuckooHeader> header) {
    // Calculate keys
    SiphashKeys keys;
    {
        std::vector<uint8_t> h = header->prePow;
        auto nonce = header->nonce;
        LOG(INFO) << "nonce = " << nonce;
        for (size_t i = 0; i < sizeof(nonce); ++i) {
            // little endian
            h.push_back(nonce & 0xFF);
            nonce = nonce >> 8;
        }
        uint64_t keyArray[4];
        blake2b(keyArray, sizeof(keyArray), h.data(), h.size(), 0, 0);
        keys.k0 = htole64(keyArray[0]);
        keys.k1 = htole64(keyArray[1]);
        keys.k2 = htole64(keyArray[2]);
        keys.k3 = htole64(keyArray[3]);
    }

    LOG(INFO) << "siphash keys: " << keys.k0 << ", " << keys.k1 << ", " << keys.k2 << ", " << keys.k3;

    // Init active edges bitmap:
    const uint32_t blocksize = 2 * 1024;
    kernelFillBuffer_.setArg(0, bufferActiveEdges_);
    kernelFillBuffer_.setArg(1, 0xffffffff);
    kernelFillBuffer_.setArg(2, blocksize);
    uint32_t iterations = (edgeCount_ / 8 / sizeof(uint32_t)) / blocksize;
    LOG(INFO) << "iterations=" << iterations;
    cl_int err = queue_.enqueueNDRangeKernel(kernelFillBuffer_, {}, {64 * iterations}, {64});
    checkErr(err);

    int uorv = 0;
    for(uint32_t round=0; round<pruneRounds_; ++round) {
        uint32_t active = remainingEdges[std::min(7U, round)];
        //Timer rt;
        //rt.start();
        VLOG(0) << "Round " << round << ", uorv=" << uorv;
        pruneActiveEdges(keys, active, uorv, round == 0);
        //queue_->finish();
        //VLOG(0) << "elapsed: round=" << rt.getSecondsElapsed() <<", total=" << timer.getSecondsElapsed() << "s";
        uorv ^= 1;
    }
    queue_.finish();

    uint32_t* actives = new uint32_t[edgeCount_ / 32];
    queue_.enqueueReadBuffer(bufferActiveEdges_, true, 0 /* offset */, edgeCount_ / 8, actives);
    uint32_t debugActive = 0;
    for(uint32_t i=0; i<edgeCount_ / 32; ++i) {
        debugActive += __builtin_popcount(actives[i]);
    }
    VLOG(0) << "active edges: " << debugActive;

    unique_ptr<CuckooSolution> s = header->makeWorkResult<kCuckaroo31>();
    return s;
}

void CuckatooSolver::pruneActiveEdges(const SiphashKeys& keys, uint32_t activeEdges, int uorv, bool initial) {
    if (initial) {
        kernelCreateNodes_.setArg(0, keys);
        kernelCreateNodes_.setArg(2, uorv);
    } else {
        kernelKillEdgesAndCreateNodes_.setArg(0, keys);
        kernelKillEdgesAndCreateNodes_.setArg(2, uorv);
    }

    uint32_t nodeCapacity = nodeBytes_ / sizeof(uint32_t) / 10 * 9;
    VLOG(0) << "node capacity = " << nodeCapacity;
    uint32_t nodePartitions = activeEdges / nodeCapacity + 1;
    //nodePartitions = 16;
    shared_ptr<cl::Event> accumulated = nullptr;
    const uint32_t totalWork = edgeCount_ / 32; /* Each thread processes 32 bit. */
    const uint32_t workPerPartition = (totalWork / nodePartitions) & ~2047;
    VLOG(0) << "node partitions=" << nodePartitions << ", work per partition=" << workPerPartition;

    uint32_t offset = 0;
    for(uint32_t partition = 0; partition < nodePartitions; ++partition) {
        fillBuffer(queue_, bufferCounters_, 0 /* pattern */, 0 /* offset */, 4 * buckets_);

        uint32_t work;
        if (partition == nodePartitions - 1) {
            work = totalWork - offset;
        } else {
            work = workPerPartition;
        }
        //VLOG(0) << partition << ": " << offset << ", " << work;
        if (initial) {
            queue_.enqueueNDRangeKernel(kernelCreateNodes_, {offset}, {work}, {64});
        } else {
            queue_.enqueueNDRangeKernel(kernelKillEdgesAndCreateNodes_, {offset}, {work}, {64});
        }

        offset += workPerPartition;

        //context_->asyncPrintEventTimings(created.get(), "CreateNodes");
        kernelAccumulateNodes_.setArg(4, (partition == 0) ? 1 : 0);
        queue_.enqueueNDRangeKernel(kernelAccumulateNodes_, {}, {buckets_ * 256}, {256});
    }

    queue_.enqueueNDRangeKernel(kernelCombineActiveNodes_, {}, {edgeCount_ / 64}, {64});
}

void CuckatooSolver::prepare() {
    CLProgramLoader& programLoader = opts_.programLoader;
    uint32_t bucketBitShift = getBucketBitShift();
    LOG(INFO) << "Bucket bit shift = " << bucketBitShift;
    cl_int err;
    int64_t availableBytes = opts_.device.getInfo<CL_DEVICE_GLOBAL_MEM_SIZE>(&err);
    if (err) {
        LOG(ERROR) << "Failed to get available memory count.";
        return;
    }
    int64_t maxMemoryAlloc = opts_.device.getInfo<CL_DEVICE_MAX_MEM_ALLOC_SIZE>(&err);
    checkErr(err);

    int64_t bitmapBytes = (edgeCount_ / 8) * 5 / 2;
    int64_t availableNodeBytes = availableBytes - bitmapBytes;
    int64_t nodeBytes = availableNodeBytes - 300* 1024 * 1024;
    nodeBytes = (1ULL << 31) /9*11;
    nodeBytes = std::min(nodeBytes, maxMemoryAlloc);
    nodeBytes_ = nodeBytes;
    VLOG(0) << "Bytes: total=" << availableBytes << ", bitmaps=" << bitmapBytes << ", nodes=" << nodeBytes;

    buckets_ = edgeCount_ >> bucketBitShift;
    maxBucketSize_ = nodeBytes / 4  / buckets_;
    maxBucketSize_ = (maxBucketSize_ & (~31));
    VLOG(0) << buckets_ << " buckets with max size " << maxBucketSize_;

    std::vector<cstring_span> files;
    files.push_back("grin/siphash.h");
    files.push_back("grin/cuckatoo.cl");

    // TODO proper error handling

    std::string options = "-DBUCKET_BIT_SHIFT=" + std::to_string(bucketBitShift);
    program_ = programLoader.loadProgram(opts_.context, files, options).value();

    bufferActiveEdges_ = cl::Buffer(opts_.context, CL_MEM_READ_WRITE, edgeCount_ / 8);
    bufferActiveNodes_ = cl::Buffer(opts_.context, CL_MEM_READ_WRITE, edgeCount_ / 8);
    bufferActiveNodesCombined_ = cl::Buffer(opts_.context, CL_MEM_READ_WRITE, edgeCount_ / 16);
    bufferNodes_ = cl::Buffer(opts_.context, CL_MEM_READ_WRITE, nodeBytes);
    bufferCounters_ = cl::Buffer(opts_.context, CL_MEM_READ_WRITE, 4 * buckets_);

    // CreateNodes
    kernelCreateNodes_ = cl::Kernel(program_, "CreateNodes");
    kernelCreateNodes_.setArg(1, bufferActiveEdges_);
    kernelCreateNodes_.setArg(3 /* nodeMask */, edgeCount_ - 1);
    kernelCreateNodes_.setArg(4, bufferNodes_);
    kernelCreateNodes_.setArg(5, bufferCounters_);
    kernelCreateNodes_.setArg(6, maxBucketSize_);

    // AccumulateNodes
    kernelAccumulateNodes_ = cl::Kernel(program_, "AccumulateNodes");
    kernelAccumulateNodes_.setArg(0, bufferNodes_);
    kernelAccumulateNodes_.setArg(1, bufferCounters_);
    kernelAccumulateNodes_.setArg(2, maxBucketSize_);
    kernelAccumulateNodes_.setArg(3, bufferActiveNodes_);

    // CombineActive
    kernelCombineActiveNodes_ = cl::Kernel(program_, "CombineActiveNodes");
    kernelCombineActiveNodes_.setArg(0, bufferActiveNodes_);
    kernelCombineActiveNodes_.setArg(1, bufferActiveNodesCombined_);

    // KillEdges
    kernelKillEdgesAndCreateNodes_ = cl::Kernel(program_, "KillEdgesAndCreateNodes");
    kernelKillEdgesAndCreateNodes_.setArg(1, bufferActiveNodesCombined_);
    kernelKillEdgesAndCreateNodes_.setArg(3 /* nodeMask */, edgeCount_ - 1);
    kernelKillEdgesAndCreateNodes_.setArg(4, bufferActiveEdges_);
    kernelKillEdgesAndCreateNodes_.setArg(5, bufferNodes_);
    kernelKillEdgesAndCreateNodes_.setArg(6, bufferCounters_);
    kernelKillEdgesAndCreateNodes_.setArg(7, maxBucketSize_);

    // FillBuffer
    kernelFillBuffer_ = cl::Kernel(program_, "FillBuffer");

    queue_ = cl::CommandQueue(opts_.context);
}

int32_t CuckatooSolver::getBucketBitShift() {
    if (opts_.vendor == VendorEnum::kAMD) {
        return 19;
    }
    uint32_t localMem = opts_.device.getInfo<CL_DEVICE_LOCAL_MEM_SIZE>();
    uint32_t bucketBitShift = 8;
    while((1 << bucketBitShift) < localMem) {
        bucketBitShift *= 2;
    }
    return bucketBitShift;
}

} /* namespace miner */
