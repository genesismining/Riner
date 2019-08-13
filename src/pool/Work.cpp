
#include "Work.h"
#include "PoolWithWorkQueue.h"

namespace miner {

    bool PoolJob::expired() const {
        bool expired = true;
        if (auto sharedPtr = pool.lock()) {
            expired = sharedPtr->latestJobId.load(std::memory_order_relaxed) != id;
        }
        return expired;
    }

    bool PoolJob::valid() const {
        return !pool.expired();
    }

    WorkSolution::WorkSolution(const Work &work)
            : job(work.job), powType(work.powType) {
    }

    Work::Work(const std::string &powType)
            : powType(powType) {
    }

}