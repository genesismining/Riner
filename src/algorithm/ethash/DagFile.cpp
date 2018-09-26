

#include "DagFile.h"

namespace miner {

    uint32_t DagFile::getEpoch() const {
        return epoch;
    }

    DagFile::operator bool() const {
        return valid;
    }

    void DagFile::generate(uint32_t epoch, ByteSpan<32> seedHash, cl::Device &device) {
        valid = true;
    }

}