
#include "Work.h"
#include "WorkQueue.h"

namespace riner {

    PoolJob::PoolJob(std::weak_ptr<Pool> pool)
        : pool(std::move(pool)) {
    }

    bool PoolJob::expired() const {
        bool expired = true;
        if (auto sharedPtr = pool.lock()) {
            expired = sharedPtr->isExpiredJob(*this);
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