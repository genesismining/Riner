#pragma once

#include "siphash.h"

#include <src/util/Copy.h>
#include <src/util/ConfigUtils.h>
#include <src/pool/WorkCuckoo.h>

#include <src/common/OpenCL.h>
#include <src/compute/opencl/CLProgramLoader.h>
#include <src/compute/ComputeApiEnums.h>
#include <stdint.h>
#include <string>

namespace miner {

class CuckatooSolver {
public:
    typedef std::function<bool()> AbortFn;

    struct Options {
        Device &deviceInfo;
        uint32_t n = 0;
        uint32_t cycleLength = 42;
        cl::Context context;
        cl::Device device;
        CLProgramLoader* programLoader = nullptr;
    };

    struct Cycle {
        std::vector<uint32_t> edges;
    };

    CuckatooSolver(Options options) :
            opts_(std::move(options)),
            edgeCount_(static_cast<uint32_t>(1) << opts_.n) {
        CHECK(opts_.cycleLength % 2 == 0) << "Cycle length must be even!";
        prepare();
    }

    DELETE_COPY(CuckatooSolver);

    inline Device &getDevice() {
        return opts_.deviceInfo;
    }

    std::vector<Cycle> solve(const SiphashKeys& keys, AbortFn abortFn);

    static bool isValidCycle(uint32_t n, uint32_t cycleLength, const SiphashKeys& keys, const Cycle& cycle);

private:
    void prepare();

    void pruneActiveEdges(const SiphashKeys& keys, uint32_t activeEdges, int uorv, bool initial);

    int32_t getBucketBitShift();

    uint32_t getNode(const SiphashKeys& keys, uint32_t edge, uint32_t uOrV);

    std::string getDeviceName();

    static constexpr uint32_t pruneRounds_ = 99;

    const Options opts_;
    const uint32_t edgeCount_;

    uint32_t buckets_ = 0;
    uint32_t maxBucketSize_ = 0;
    int64_t nodeBytes_ = -1;

    cl::Program program_;

    cl::Buffer bufferActiveEdges_;
    cl::Buffer bufferActiveNodes_;
    cl::Buffer bufferActiveNodesCombined_;
    cl::Buffer bufferNodes_;
    cl::Buffer bufferCounters_;

    cl::Kernel kernelFillBuffer_;
    cl::Kernel kernelCreateNodes_;
    cl::Kernel kernelAccumulateNodes_;
    cl::Kernel kernelCombineActiveNodes_;
    cl::Kernel kernelKillEdgesAndCreateNodes_;

    cl::CommandQueue queue_;
};

} /* namespace miner */

