
#pragma once

#include <mutex>
#include <src/common/OpenCL.h>
#include <src/common/Span.h>

namespace miner {

    class DagFile {

        const uint32_t epoch = 0;
        const uint32_t allocationMaxEpoch = 0; //the epoch that is used for
        // allocating the dag memory. It is chosen larger than current epoch, so
        // that the dag can be recreated several times for a new epoch before a
        // new allocation is necessary

        cl::Memory dagBuffer;

        bool valid = false;
    public:
        void generate(uint32_t epoch, ByteSpan<32> seedHash, const cl::Device &device);

        uint32_t getEpoch() const;

        operator bool() const;
    };

    class DagCacheContainer {

    };

}
