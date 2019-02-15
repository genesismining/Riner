#pragma once

#include "siphash.h"

#include <src/util/Copy.h>
#include <src/pool/Work.h>

#include <src/common/OpenCL.h>
#include <src/compute/opencl/CLProgramLoader.h>
#include <src/compute/ComputeApiEnums.h>
#include <stdint.h>
#include <string>
#include <lib/cl2hpp/include/cl2.hpp>

namespace miner {

typedef Work<kCuckaroo31> CuckooHeader;
typedef WorkResult<kCuckaroo31> CuckooSolution;


class CuckatooSolver {
public:
    struct Options {
        Options(CLProgramLoader& programLoader): programLoader(programLoader) {}

        uint32_t n = 0;
        cl::Context context;
        cl::Device device;
        VendorEnum vendor = VendorEnum::kVendorEnumCount;
        CLProgramLoader& programLoader;
    };

    CuckatooSolver(Options options) :
            opts_(std::move(options)),
            edgeCount_(static_cast<uint32_t>(1) << opts_.n) {
        prepare();
    }

    DELETE_COPY_AND_ASSIGNMENT(CuckatooSolver);

    std::unique_ptr<CuckooSolution> solve(std::unique_ptr<CuckooHeader> header);

private:
    void prepare();

    void pruneActiveEdges(const SiphashKeys& header, uint32_t activeEdges, int uorv, bool initial);

    int32_t getBucketBitShift();

    static constexpr uint32_t pruneRounds_ = 20;

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

