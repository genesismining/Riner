
#pragma once

#include <mutex>
#include <src/common/OpenCL.h>
#include <src/pool/WorkEthash.h>
#include <src/common/Optional.h>
#include <src/common/Span.h>
#include <src/algorithm/ethash/DagCache.h>

namespace riner {
    
    /**
     * Utility for generating the ethash DagFile on the gpu via opencl
     * see the generate method.
     */
    class DagFile {

        uint32_t epoch = std::numeric_limits<uint32_t>::max();
        uint32_t allocationMaxEpoch = 0; //the epoch that is used for
        // allocating the dag memory. It is chosen larger than current epoch, so
        // that the dag can be recreated several times for a new epoch before a
        // new allocation is necessary

        cl::Buffer clDagBuffer;
        uint32_t size = 0;

        bool valid = false;
    public:
        bool generate(const DagCacheContainer &cache,
                const cl::Context &, const cl::Device &, cl::Program &generateDagProgram);

        uint32_t getEpoch() const;

        cl::Buffer &getCLBuffer();
        uint64_t getByteSize() const;
        uint32_t getSize() const;

        static uint64_t getByteSize(uint32_t epoch);
        static uint32_t getSize(uint32_t epoch);

        operator bool() const;
    };

}
