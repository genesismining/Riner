
#pragma once

#include <src/common/Span.h>
#include <src/util/Bytes.h>
#include <src/util/DynamicBuffer.h>

namespace riner {

    /**
     * contains Dag Caches on the CPU which are used for verification of ethash shares
     */
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

        /**
         * generates the dag cache for a given epoch and seedhash
         * WARNING: takes very long, expect the call to block for a while
         * param epoch the ethash epoch
         * param seedHash the seedHash from the ethash Work
         */
        void generate(uint32_t epoch, cByteSpan<32> seedHash);

        /**
         * return whether the dag cache was generated for `epoch`
         */
        bool isGenerated(uint32_t epoch) const;

        uint32_t getEpoch() const;

        uint32_t getSize() const;

        uint64_t getByteSize() const;

        cByteSpan<> getByteCache() const;

        HashResult getHash(const Bytes<32> &header, uint64_t nonce) const;

        operator bool() const;
    };

    /**
     * rather expensive function that calculates the ethash epoch by hashing 0 until the provided seedhash is reached.
     * return the Ethash Epoch number, or std::numeric_limits<uint32_t>::max() if the hash could not be reproduced
     */
    uint32_t calculateEthEpoch(cByteSpan<32> seedHash);

}
