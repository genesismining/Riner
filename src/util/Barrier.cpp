//
//

#include "Barrier.h"

namespace miner {

    std::shared_future<void> Barrier::copyFuture() const {
        //we are not allowed to call wait()/wait_for() from multiple threads on a shared_future simultaneously, therefore we must copy it first
        std::lock_guard<std::mutex> lock{copyFutureMutex};
        return sharedFuture;
    }

    Barrier::Barrier() : sharedFuture(promise.get_future()) {
    }

}