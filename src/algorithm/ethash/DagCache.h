
#pragma once

#include <src/common/Span.h>
#include <src/util/Bytes.h>
#include <src/util/DynamicBuffer.h>

namespace miner {

    class DagCacheContainer {

        uint32_t currentEpoch = std::numeric_limits<uint32_t>::max();
        bool valid = false;

        union alignas(64) node_t {
            uint8_t byte[64];
            uint32_t word[16];
        };

        DynamicBuffer<node_t> buffer;

        node_t getDagItem(uint32_t idx) const;

    public:

        struct HashResult {
            Bytes<32> proofOfWorkHash;
            Bytes<32> mixHash;
        };

        void generate(uint32_t epoch, cByteSpan<32> seedHash);

        bool isGenerated(uint32_t epoch) const;

        uint32_t getEpoch() const;

        uint32_t getSize() const;

        uint64_t getByteSize() const;

        cByteSpan<> getByteCache() const;

        HashResult getHash(const Bytes<32> &header, uint64_t nonce) const;

        operator bool() const;
    };

    uint32_t calculateEthEpoch(cByteSpan<32> seedHash);

}
