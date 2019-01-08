
#pragma once

#include <mutex>
#include <src/common/OpenCL.h>
#include <src/pool/Work.h>
#include <src/common/Optional.h>
#include <src/common/Span.h>
#include <src/algorithm/ethash/DagCache.h>

namespace miner {

    class DagFile {

        uint32_t epoch = 0;
        uint32_t allocationMaxEpoch = 0; //the epoch that is used for
        // allocating the dag memory. It is chosen larger than current epoch, so
        // that the dag can be recreated several times for a new epoch before a
        // new allocation is necessary

        cl::Buffer clDagBuffer;
        size_t size = 0;

        bool valid = false;
    public:

        bool generate(const DagCacheContainer &cache, cByteSpan<32> seedHash,
                const cl::Context &, const cl::Device &, cl::Program &generateDagProgram);

        uint32_t getEpoch() const;

        cl::Buffer &getCLBuffer();
        size_t getSize() const;

        operator bool() const;
    };

}
