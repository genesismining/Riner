//
//

#include "PoolSwitcher.h"
#include "WorkQueue.h"
#include <src/common/Assert.h>
#include <src/algorithm/Algorithm.h>

namespace miner {


    PoolSwitcher::PoolSwitcher(std::string powType, clock::duration checkInterval, clock::duration durUntilDeclaredDead)
            : Pool(PoolConstructionArgs{})
            , checkInterval(checkInterval)
            , durUntilDeclaredDead(durUntilDeclaredDead) {

        periodicAliveCheckTask = std::async(std::launch::async, &PoolSwitcher::periodicAliveCheck, this);
    }

    PoolSwitcher::~PoolSwitcher() {
        {
            std::lock_guard<std::mutex> lock(mut);
            shutdown = true;
        }
        notifyOnShutdown.notify_all();
    }

    void PoolSwitcher::periodicAliveCheck() {
        std::unique_lock<std::mutex> lock(mut);

        while(!shutdown) {

            if (!pools.empty()) {
                LOG(INFO) << "checking pool connection status";
                aliveCheckAndMaybeSwitch();
            }

            //use condition vairable to wait, so the wait can be interrupted on shutdown
            notifyOnShutdown.wait_for(lock, checkInterval, [this] {
                return shutdown;
            });
        }
    }

    void PoolSwitcher::aliveCheckAndMaybeSwitch() {
        auto now = clock::now();

        for (size_t i = 0; i < pools.size(); ++i) {
            auto pool = pools[i];

            auto lastKnownAliveTime = pool->getLastKnownAliveTime();

            if (i <= activePoolIndex) {
                bool dead = now - lastKnownAliveTime > durUntilDeclaredDead;
                if (dead) {
                    if (i == activePoolIndex) {
                        ++activePoolIndex;

                        LOG(INFO) << "Pool #" << i << " is inactive, trying next backup pool";
                    }
                }
                else {
                    //if alive, but different pool => assign as new active pool
                    if (activePoolIndex != i) {
                        activePoolIndex = i;

                        auto name = gsl::to_string(pool->getName());
                        LOG(INFO) << "Pool #" << i << " (" << name << ") chosen as new active pool";
                    }
                }
            }
        }

        if (activePoolIndex == pools.size()) {
            LOG(INFO) << "no more backup pools available. Waiting for pools to become available again.";
        }
    }

    std::shared_ptr<Pool> PoolSwitcher::activePool() {
        if (activePoolIndex >= pools.size())
            return nullptr;
        auto pool = pools[activePoolIndex];
        MI_EXPECTS(pool != nullptr);
        return pool;
    }

    unique_ptr<Work> PoolSwitcher::tryGetWorkImpl() {
        std::lock_guard<std::mutex> lock(mut);
        if (auto pool = activePool()) {
            LOG(INFO) << "getting work from " << gsl::to_string(pool->getName());
            return pool->tryGetWorkImpl();
        }

        LOG(INFO) << "PoolSwitcher cannot provide work since there is no active pool";
        //wait for a short period of time to prevent busy waiting in the algorithms' loops
        //TODO: better solution would be to wait for a short time, and if a pool becomes available in that time period it can be used immediately and the function returns
        std::this_thread::sleep_for(std::chrono::seconds(1));
        return nullptr;
    }

    void PoolSwitcher::submitSolutionImpl(unique_ptr<WorkSolution> solution) {
        std::shared_ptr<const PoolJob> job = solution->getJob();
        if (!job) {
            LOG(INFO) << "work solution is not submitted because its job is stale";
            return;
        }
        std::shared_ptr<Pool> pool = job->pool.lock();
        if (!pool) {
            LOG(INFO) << "work solution cannot be submitted because its pool does not exist anymore";
            return;
        }
        
        auto solutionPoolUid = pool->poolUid;
        auto activePoolUid = std::numeric_limits<decltype(solutionPoolUid)>::max();
        bool sameUid = true;

        {
            std::lock_guard<std::mutex> lock(mut);

            if (auto pool_ = activePool()) {

                activePoolUid = pool_->poolUid;

                sameUid = activePoolUid == solutionPoolUid;
                if (sameUid) {
                    return pool_->submitSolutionImpl(std::move(solution));
                }
            }
        } //unlock

        if (sameUid)
            LOG(INFO) << "solution could not be submitted, since there is no active pool";
        else
            LOG(INFO) << "solution belongs to another pool (uid " << solutionPoolUid << ") and will not be submitted to current pool (uid " << activePoolUid << ")";
    }

    cstring_span PoolSwitcher::getName() const {
        return "PoolSwitcher at " + std::to_string(uintptr_t(this));
    }

    size_t PoolSwitcher::poolCount() const {
        std::lock_guard<std::mutex> lock(mut);
        return pools.size();
    }

}
