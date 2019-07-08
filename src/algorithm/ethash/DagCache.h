
#pragma once

#include <src/common/WorkCommon.h>
#include <src/common/Span.h>
#include <src/util/DynamicBuffer.h>

namespace miner {

    class DagCacheContainer {

        uint32_t currentEpoch = std::numeric_limits<uint32_t>::max();
        bool valid = false;

        DynamicBuffer buffer;

    public:
        void generate(uint32_t epoch, cByteSpan<32> seedHash);

        bool isGenerated(uint32_t epoch) const;

        uint32_t getEpoch() const;

        cByteSpan<> getCache() const;

        operator bool() const;
    };

}