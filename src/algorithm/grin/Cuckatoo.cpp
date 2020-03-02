#include <src/algorithm/grin/Cuckatoo.h>

#include "Graph.h"

#include <src/common/Endian.h>
#include <src/pool/WorkCuckoo.h>
#include <src/util/Logging.h>
#include <src/util/StringUtils.h>
#include <src/util/TaskExecutorPool.h>

#include <vector>

namespace miner {

namespace {
constexpr uint64_t remainingEdges[8] =
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
#ifdef CL_VERSION_1_2
    cl_int err = queue.enqueueFillBuffer(buffer, pattern, bufferOffset, size);
    checkErr(err);
#else
    std::vector<uint8_t> memory(size, {0});
    queue.enqueueWriteBuffer(buffer, CL_FALSE, bufferOffset, size, memory.data());
#endif
}

void foreachActiveEdge(uint32_t n, const uint32_t* edges, const std::function<void(uint32_t)> &f) {
    for (uint32_t i = 0; i < (uint32_t(1) << (n - 5)); ++i) {
        uint32_t bits = edges[i];
        while (bits != 0) {
            int b = 31 - __builtin_clz(bits);
            bits ^= 1 << b;
            uint32_t edge = 32U * i + b;
            f(edge);
        }
    }
}


}  // namespace

const std::future<void> &CuckatooSolver::solve(const SiphashKeys &keys, ResultFn resultFn, AbortFn abortFn) {
    VLOG(0) << "Siphash Keys: " << keys.k0 << ", " << keys.k1 << ", " << keys.k2 << ", " << keys.k3;

    // Init active edges bitmap:
    const uint32_t blocksize = 2 * 1024;
    kernelFillBuffer_.setArg(0, bufferActiveEdges_);
    kernelFillBuffer_.setArg(1, 0xffffffff);
    kernelFillBuffer_.setArg(2, blocksize);
    uint32_t iterations = uint32_t(edgeCount_ / 32) / blocksize;
    LOG(INFO) << "iterations=" << iterations;
    cl_int err = queue_.enqueueNDRangeKernel(kernelFillBuffer_, {}, {64 * iterations}, {64});
    checkErr(err);

    int uorv = 0;
    for (uint32_t round = 0; round < pruneRounds_; ++round) {
        uint64_t active = remainingEdges[std::min(7U, round)];
        VLOG(2) << "Round " << round << ", uorv=" << uorv;
        pruneActiveEdges(keys, active, uorv, round == 0);
        queue_.flush();
        uorv ^= 1;
        if (abortFn()) {
            VLOG(0) << "Aborting solve.";
            return taskFuture;
        }
    }
    queue_.finish();
    if (abortFn()) {
        VLOG(0) << "Aborting solve.";
        return taskFuture;
    }
    VLOG(0) << "Done";

    // TODO This is not very efficient. Would be better to just copy a list of edges. Or do all on GPU.
    //      Of course we can also do this in parallel to anything on the GPU.
    std::vector<uint32_t> edges(edgeCount_ / 32);
    queue_.enqueueReadBuffer(bufferActiveEdges_, CL_TRUE, 0 /* offset */, edgeCount_ / 8, edges.data());

    // wait for previous task so that the CPU cannot become overloaded
    // this might waste compute power when the number of CPU cores is larger than the number of GPUs
    if (taskFuture.valid()) {
        taskFuture.wait();
    }
    taskFuture = opts_.tasks.addTask([this, keys, edges = std::move(edges), resultFn = std::move(resultFn)] () -> void {
        uint32_t debugActive = 0;

        VLOG(1) << "Bulding Graph";
        Graph graph(opts_.n, opts_.n - 13, opts_.n - 13); // TODO this should be determined by the estimate of remaining edges

        miner::foreachActiveEdge(opts_.n, edges.data(), [this, &keys, &graph, &debugActive](uint32_t edge) {
            uint32_t u = getNode(keys, edge, 0);
            uint32_t v = getNode(keys, edge, 1);
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
            for (auto &cycle: cycles) {
                if (cycle.uvs.size() != 2 * opts_.cycleLength) {
                    LOG(ERROR) << "Invalid uvs size: " << cycle.uvs.size();
                    cycle.uvs.clear();
                }
                cycle.edges.resize(opts_.cycleLength, 0);
            }
            foreachActiveEdge(opts_.n, edges.data(), [this, &keys, &cycles](uint32_t edge) {
                uint32_t u = getNode(keys, edge, 0);
                uint32_t v = getNode(keys, edge, 1);
                for (auto &cycle: cycles) {
                    for (size_t i = 0; i < opts_.cycleLength; i++) {
                        if (u == cycle.uvs.at(2 * i) && v == cycle.uvs.at(2 * i + 1)) {
                            cycle.edges.at(i) = edge;
                        }
                    }
                }
            });
            for (auto &cycle: cycles) {
                std::sort(cycle.edges.begin(), cycle.edges.end());
                if (cycle.edges[0] == cycle.edges[1]) {
                    // Not a true cycle of full length. TODO do the check before resolving edges
                    continue;
                }
                Cycle c;
                c.edges = std::move(cycle.edges);
                if (!isValidCycle(opts_.n, opts_.cycleLength, keys, c)) {
                    LOG(ERROR) << "GPU " << getDeviceName() << " produced invalid cycle [" << toString(cycle.edges)
                               << "]!";
                }
                result.push_back(std::move(c));
            }

        }
        if (!result.empty()) {
            LOG(INFO) << "Found " << result.size() << " full cycles";
        }
        resultFn(std::move(result));
    });
    return taskFuture;
}

void CuckatooSolver::pruneActiveEdges(const SiphashKeys& keys, uint64_t activeEdges, int uorv, bool initial) {
    if (initial) {
        kernelCreateNodes_.setArg(0, keys);
        kernelCreateNodes_.setArg(2, uorv);
    } else {
        kernelKillEdgesAndCreateNodes_.setArg(0, keys);
        kernelKillEdgesAndCreateNodes_.setArg(2, uorv);
    }

    uint32_t nodeCapacity = nodeBytes_ / sizeof(uint32_t) / 10 * 9;
    VLOG(1) << "node capacity = " << nodeCapacity;
    auto nodePartitions = uint32_t(activeEdges / nodeCapacity + 1);
    const auto totalWork = uint32_t(edgeCount_ / 32); /* Each thread processes 32 bit. */
    const uint32_t workPerPartition = (totalWork / nodePartitions) & ~2047;
    VLOG(3) << "node partitions=" << nodePartitions << ", work per partition=" << workPerPartition;

    uint32_t offset = 0;
    for (uint32_t partition = 0; partition < nodePartitions; ++partition) {
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

    buckets_ = uint32_t(edgeCount_ >> bucketBitShift);
    maxBucketSize_ = uint32_t(nodeBytes / 4  / buckets_);
    maxBucketSize_ = (maxBucketSize_ & (~31));
    VLOG(0) << buckets_ << " buckets with max size " << maxBucketSize_;

    std::vector<std::string> files;
    files.emplace_back("/kernel/siphash.h");
    files.emplace_back("/kernel/cuckatoo.cl");

    // TODO proper error handling

    std::string options = "-DBUCKET_BIT_SHIFT=" + std::to_string(bucketBitShift);
    MI_EXPECTS(opts_.programLoader);
    auto programOr = opts_.programLoader->loadProgram(opts_.context, files, options);
    MI_EXPECTS(programOr);
    program_ = programOr.value();

    bufferActiveEdges_ = cl::Buffer(opts_.context, CL_MEM_READ_WRITE, edgeCount_ / 8);
    bufferActiveNodes_ = cl::Buffer(opts_.context, CL_MEM_READ_WRITE, edgeCount_ / 8);
    bufferActiveNodesCombined_ = cl::Buffer(opts_.context, CL_MEM_READ_WRITE, edgeCount_ / 16);
    bufferNodes_ = cl::Buffer(opts_.context, CL_MEM_READ_WRITE, nodeBytes);
    bufferCounters_ = cl::Buffer(opts_.context, CL_MEM_READ_WRITE, 4 * buckets_);

    // CreateNodes
    kernelCreateNodes_ = cl::Kernel(program_, "CreateNodes");
    kernelCreateNodes_.setArg(1, bufferActiveEdges_);
    kernelCreateNodes_.setArg(3, nodeMask_);
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
    kernelKillEdgesAndCreateNodes_.setArg(3, nodeMask_);
    kernelKillEdgesAndCreateNodes_.setArg(4, bufferActiveEdges_);
    kernelKillEdgesAndCreateNodes_.setArg(5, bufferNodes_);
    kernelKillEdgesAndCreateNodes_.setArg(6, bufferCounters_);
    kernelKillEdgesAndCreateNodes_.setArg(7, maxBucketSize_);

    // FillBuffer
    kernelFillBuffer_ = cl::Kernel(program_, "FillBuffer");

    queue_ = cl::CommandQueue(opts_.context);
}

int32_t CuckatooSolver::getBucketBitShift() {
    uint32_t localMem = opts_.device.getInfo<CL_DEVICE_LOCAL_MEM_SIZE>();
    VLOG(1) << "local mem: " << localMem << "bytes";
    uint32_t bucketBitShift = 1;
    while((uint32_t(1) << bucketBitShift) <= localMem * 8) {
        bucketBitShift++;
    }
    return bucketBitShift - 1;
}

/* static */ bool CuckatooSolver::isValidCycle(uint32_t n, uint32_t cycleLength, const SiphashKeys& keys, const Cycle& cycle) {
    if (cycle.edges.size() != cycleLength) {
        return false;
    }
    // Edges must be strictly ascending.
    for (uint32_t i = 1; i < cycleLength; ++i) {
        if (cycle.edges[i] <= cycle.edges[i - 1]) {
            return false;
        }
    }
    // Edges must not be in range [0..2^n-1].
    const auto nodemask = uint32_t((uint64_t(1) << n) - 1);
    if (cycle.edges.back() > nodemask) {
        return false;
    }

    // Build maps u->v and v->u.
    // TODO This excludes cycles that go through the same u and v twice but on different edges.
    std::unordered_map<uint32_t, uint32_t> uToV;
    std::unordered_map<uint32_t, uint32_t> vToU;

    for(uint32_t edge: cycle.edges) {
        uint32_t u = siphash24(&keys, (2 * edge) | 0) & nodemask;
        uint32_t v = siphash24(&keys, (2 * edge) | 1) & nodemask;
        uToV[u] = v;
        vToU[v] = u;
    }

    // Follow cycle length edges starting at any u.
    // We remove edges from the maps to make sure they only get used once.
    uint32_t start = uToV.begin()->first;
    uint32_t u = start;
    for (uint32_t i = 0; i < cycleLength / 2; ++i) {
        auto it = uToV.find(u);
        if (it == uToV.end()) {
            return false;
        }
        uint32_t v = it->second;
        uToV.erase(it);
        vToU.erase(v);
        v ^= 1;

        it = vToU.find(v);
        if (it == vToU.end()) {
            return false;
        }
        u = it->second;
        vToU.erase(it);
        uToV.erase(u);
        u ^= 1;
    }

    CHECK(uToV.empty());
    CHECK(vToU.empty());

    return u == start;
}

const std::string & CuckatooSolver::getDeviceName() {
    return opts_.deviceInfo.id.getName();
}

} /* namespace miner */
