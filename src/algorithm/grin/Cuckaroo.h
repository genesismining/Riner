#pragma once

#include "siphash.h"

#include <src/util/Copy.h>
#include <src/pool/Work.h>

#include <src/common/OpenCL.h>
#include <src/compute/opencl/CLProgramLoader.h>
#include <stdint.h>
#include <string>
#include <lib/cl2hpp/include/cl2.hpp>

namespace miner {

typedef Work<kCuckaroo31> CuckooHeader;
typedef WorkResult<kCuckaroo31> CuckooSolution;


class CuckatooSolver {
public:
    CuckatooSolver(uint32_t n, cl::Context context, cl::Device device, CLProgramLoader& programLoader) :
            n_(n),
            edgeCount_(static_cast<uint32_t>(1) << n_),
            context_(context), device_(device) {
        prepare(programLoader);
    }

    DELETE_COPY_AND_ASSIGNMENT(CuckatooSolver);

    std::unique_ptr<CuckooSolution> solve(std::unique_ptr<CuckooHeader> header);

private:
    void prepare(CLProgramLoader& programLoader);

    void pruneActiveEdges(const CuckooHeader& header, uint32_t activeEdges, int uorv, bool initial);

    static constexpr uint32_t bucketBitShift_ = 19;
    static constexpr uint32_t pruneRounds_ = 20;

    const uint32_t n_;
    const uint32_t edgeCount_;
    const cl::Context context_;
    const cl::Device device_;

    uint32_t buckets_;
    uint32_t maxBucketSize_;

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

