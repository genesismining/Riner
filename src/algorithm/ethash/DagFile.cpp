

#include "DagFile.h"
#include "Ethash.h"

namespace miner {

    uint32_t DagFile::getEpoch() const {
        return epoch;
    }

    DagFile::operator bool() const {
        return valid;
    }

    void DagFile::generate(uint32_t epoch, cByteSpan<32> seedHash, const cl::Device &device) {
        valid = true;

    }





}