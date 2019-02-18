#include <src/algorithm/grin/Cuckatoo.h>

#include "Graph.h"

#include <src/common/Endian.h>
#include <src/pool/WorkCuckaroo.h>
#include <src/util/Logging.h>

#include <string>
#include <vector>

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
    // TODO
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

void foreachActiveEdge(uint32_t n, uint32_t* edges, std::function<void(uint32_t)> f) {
    const uint32_t edgeCount = static_cast<uint32_t>(1) << n;
    const uint32_t nodemask = (edgeCount - 1);
    for (uint32_t i = 0; i < edgeCount / 32; ++i) {
        uint32_t bits = edges[i];
        while (bits != 0) {
            int b = 31 - __builtin_clz(bits);
            bits ^= 1 << b;
            uint32_t edge = 32 * (uint64_t) i + b;
            f(edge);
        }
    }
}


}  // namespace

using std::shared_ptr;
using std::unique_ptr;
using std::make_unique;
using std::make_shared;

std::vector<CuckatooSolver::Cycle> CuckatooSolver::solve(SiphashKeys keys) {
    VLOG(0) << "Siphash Keys: " << keys.k0 << ", " << keys.k1 << ", " << keys.k2 << ", " << keys.k3;

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
        VLOG(2) << "Round " << round << ", uorv=" << uorv;
        pruneActiveEdges(keys, active, uorv, round == 0);
        //queue_->finish();
        //VLOG(0) << "elapsed: round=" << rt.getSecondsElapsed() <<", total=" << timer.getSecondsElapsed() << "s";
        uorv ^= 1;
    }
    queue_.finish();

    // TODO This is not very efficient. Would be better to just copy a list of edges. Or do all on GPU.
    //      Of course we can also do this in parallel to anything on the GPU.
    std::vector<uint32_t> edges;
    edges.resize(edgeCount_ / 32);
    queue_.enqueueReadBuffer(bufferActiveEdges_, true, 0 /* offset */, edgeCount_ / 8, edges.data());
    uint32_t debugActive = 0;

    Graph graph(opts_.n, opts_.n - 7, opts_.n - 7); // TODO this should be determined by the estimate of remaining edges

    foreachActiveEdge(opts_.n, edges.data(), [this, &keys, &graph, &debugActive](uint32_t edge) {
            uint32_t nodemask = edgeCount_ - 1;
            uint32_t u = siphash24(&keys, 2 * edge + 0) & nodemask;
            uint32_t v = siphash24(&keys, 2 * edge + 1) & nodemask;
            graph.addUToV(u, v);
            graph.addVToU(v, u);
            debugActive++;
    });

    VLOG(0) << "active edges: " << debugActive;

    if ((pruneRounds_ % 2) == 0) {
        graph.pruneFromV();
    } else {
        graph.pruneFromU();
    }
    LOG(INFO) << "pruning is done";
    std::vector<Graph::Cycle> cycles = graph.findCycles(opts_.cycleLength);
    LOG(INFO) << "Found " << cycles.size() << " potential cycles";

    std::vector<Cycle> result;
    if (!cycles.empty()) {
        for(auto& cycle: cycles) {
            cycle.edges.resize(opts_.cycleLength, 0);
        }
        foreachActiveEdge(opts_.n, edges.data(), [this, &keys, &cycles](uint32_t edge) {
            uint32_t nodemask = edgeCount_ - 1;
            uint32_t u = siphash24(&keys, 2 * edge + 0) & nodemask;
            uint32_t v = siphash24(&keys, 2 * edge + 1) & nodemask;
            for(auto& cycle: cycles) {
                for(size_t i = 0; i < cycle.uvs.size(); i++) {
                    if (u == cycle.uvs[2*i] && v == cycle.uvs[2*i+1]) {
                        cycle.edges[i] = edge;
                    }
                }
            }
        });
        for(auto& cycle: cycles) {
            std::sort(cycle.edges.begin(), cycle.edges.end());
            if (cycle.edges[0] == cycle.edges[1]) {
                // Not a true cycle of full length.
                continue;
            }
            Cycle c;
            c.edges = std::move(cycle.edges);
            result.push_back(std::move(c));
        }
    }
    if (!result.empty()) {
        LOG(INFO) << "Found " << result.size() << " full cycles";
    }
    return result;
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
    VLOG(1) << "node capacity = " << nodeCapacity;
    uint32_t nodePartitions = activeEdges / nodeCapacity + 1;
    //nodePartitions = 16;
    shared_ptr<cl::Event> accumulated = nullptr;
    const uint32_t totalWork = edgeCount_ / 32; /* Each thread processes 32 bit. */
    const uint32_t workPerPartition = (totalWork / nodePartitions) & ~2047;
    VLOG(1) << "node partitions=" << nodePartitions << ", work per partition=" << workPerPartition;

    uint32_t offset = 0;
    for(uint32_t partition = 0; partition < nodePartitions; ++partition) {
        fillBuffer(queue_, bufferCounters_, 0 /* pattern */, 0 /* offset */, 4 * buckets_);

        uint32_t work;
        if (partition == nodePartitions - 1) {
            work = totalWork - offset;
        } else {
            work = workPerPartition;
        }
        if (initial) {
            queue_.enqueueNDRangeKernel(kernelCreateNodes_, {offset}, {work}, {64});
        } else {
            queue_.enqueueNDRangeKernel(kernelKillEdgesAndCreateNodes_, {offset}, {work}, {64});
        }

        offset += workPerPartition;

        kernelAccumulateNodes_.setArg(4, (partition == 0) ? 1 : 0);
        queue_.enqueueNDRangeKernel(kernelAccumulateNodes_, {}, {buckets_ * 256}, {256});
    }

    queue_.enqueueNDRangeKernel(kernelCombineActiveNodes_, {}, {edgeCount_ / 64}, {64});
}

void CuckatooSolver::prepare() {
    CLProgramLoader& programLoader = opts_.programLoader;
    uint32_t bucketBitShift = getBucketBitShift();
    VLOG(0) << "Bucket bit shift = " << bucketBitShift;
    cl_int err = 0;
    cl_ulong availableBytes = opts_.device.getInfo<CL_DEVICE_GLOBAL_MEM_SIZE>(&err);
    if (err) {
        LOG(ERROR) << "Failed to get available memory count.";
        return;
    }
    cl_ulong maxMemoryAlloc = opts_.device.getInfo<CL_DEVICE_MAX_MEM_ALLOC_SIZE>(&err);
    VLOG(0) << "Max memory alloc = " << maxMemoryAlloc;
    checkErr(err);

    int64_t bitmapBytes = (edgeCount_ / 8) * 5 / 2;
    int64_t availableNodeBytes = availableBytes - bitmapBytes;
    cl_ulong nodeBytes = availableNodeBytes - 300* 1024 * 1024;
    nodeBytes = (1ULL << 31) / 9*11;
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
    VLOG(1) << "local mem: " << localMem << "bytes";
    uint32_t bucketBitShift = 1;
    while((static_cast<uint32_t>(1) << bucketBitShift) < localMem * 8) {
        bucketBitShift++;
    }
    return bucketBitShift - 1;
}

} /* namespace miner */
