
#include "DagCache.h"
#include "Ethash.h"
#include <src/util/Logging.h>

namespace miner {


    void DagCacheContainer::generate(uint32_t epoch, cByteSpan<32> seedHash) {
        valid = false;
        try {
            LOG(INFO) << "calculating dag cache for epoch = " << epoch;
            MI_EXPECTS(epoch == EthCalcEpochNumber(seedHash));
            buffer = eth_gen_cache(epoch, seedHash);

            currentEpoch = epoch;
            valid = true;
        }
        catch(std::bad_alloc &e) {
            LOG(ERROR) << "failed to allocate dag cache memory for epoch = "<< epoch <<": bad_alloc exception: " << e.what();
        }
    }

    void DagCacheContainer::generateIfNeeded(uint32_t epoch, cByteSpan<32> seedHash) {
        if (currentEpoch != epoch)
            generate(epoch, seedHash);
    }

    uint32_t DagCacheContainer::getEpoch() const {
        return currentEpoch;
    }

    DagCacheContainer::operator bool() const {
        return valid;
    }

    cByteSpan<> DagCacheContainer::getCache() const {
        return buffer.getSpan();
    }

}