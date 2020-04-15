//
//

#include "PoolSwitcher.h"
#include "WorkQueue.h"
#include <src/common/Assert.h>
#include <src/algorithm/Algorithm.h>

namespace riner {


    PoolSwitcher::PoolSwitcher(std::string powType, clock::duration checkInterval, clock::duration durUntilDeclaredDead)
            : Pool(PoolConstructionArgs{})
            , powType(powType)
            , checkInterval(checkInterval)
            , durUntilDeclaredDead(durUntilDeclaredDead) {

        RNR_EXPECTS(!powType.empty());

        periodicAliveCheckTask = std::async(std::launch::async, [this, powType] () {
            SetThreadNameStream{} << "poolswitcher " << powType;
            periodicAliveCheck();
        });
    }

    PoolSwitcher::~PoolSwitcher() {
        {
            std::lock_guard<std::mutex> lock(mut);
            shutdown = true;
        }
        notifyOnShutdown.notify_all();
    }

    void PoolSwitcher::periodicAliveCheck() {
        using namespace std::chrono;
        std::unique_lock<std::mutex> lock(mut);

        while(!shutdown) {

            if (!pools.empty()) {
                auto int_secs = duration_cast<seconds>(checkInterval).count();
                VLOG(2) << "periodic pool connection status check (every " << int_secs << "s)";
                aliveCheckAndMaybeSwitch();
            }

            //use condition vairable to wait, so the wait can be interrupted on shutdown
            notifyOnShutdown.wait_for(lock, checkInterval, [this] {
                return shutdown;
            });
        }
    }

    void PoolSwitcher::aliveCheckAndMaybeSwitch() {
        //called while holding lock this->mut
        using namespace std::chrono;
        auto now = clock::now();
        auto activePoolBefore = activePoolIndex;

        for (size_t i = 0; i < pools.size(); ++i) {
            auto pool = pools[i];

            auto lastKnownAliveTime = pool->getLastKnownAliveTime();
            auto latestDeclaredDeadTime = pool->getLatestDeclaredDeadTime();

            //make some descriptive bools
            bool isDead = now - lastKnownAliveTime > durUntilDeclaredDead;
            bool wasAlreadyDeadAndIsStillDead = (latestDeclaredDeadTime > lastKnownAliveTime) | isDead; //pool was dead last time we checked
            bool justDied = isDead && !wasAlreadyDeadAndIsStillDead;
            bool isAlive = !isDead;

            if (justDied) {//the pool died between now and the last time we checked
                pool->declareDead();

                auto int_secs = duration_cast<seconds>(durUntilDeclaredDead).count();
                if (i == activePoolIndex) {
                    ++activePoolIndex; //next pool is now the active pool
                    LOG(WARNING) << "active pool #" << i << "(" << pool->getName() << ") was inactive for " << int_secs << " sec, switching to next backup pool";
                }
                else {
                    LOG(INFO) << "currently unused backup pool #" << i << " (" << pool->getName() << ") was inactive for " << int_secs << "s";
                }
            }
            else if (wasAlreadyDeadAndIsStillDead) {//the pool was already dead last time we checked and is still dead
                if (now - latestDeclaredDeadTime > durUntilDeclaredDead) {
                    //if the declared dead time interval has passed again since the last time we declared it dead
                    //redeclare the pool dead, so it can try again to reconnect
                    pool->declareDead(); //redeclare dead to trigger re-start efforts on pool implementation site
                }

                if (i == activePoolIndex) {//pool was already dead but just became the chosen active pool because the pool on top died
                    //pass the activeIndex further down the chain
                    ++activePoolIndex;
                }
            }
            else if (isAlive) {//the pool is alive (and may have just turned alive)
                //if i is alive and a lower priority pool is active => assign i to be the new active pool
                bool activePoolWasChanged = activePoolBefore != activePoolIndex;
                if (activePoolIndex > i || (activePoolIndex == i && activePoolWasChanged)) {
                    activePoolIndex = i;
                    LOG(INFO) << "Pool #" << i << " (" << pool->getName() << ") chosen as new active pool";
                }
            }
            else {

                int test = 0;
                LOG(INFO) << test;
                RNR_EXPECTS(false); //unreachable
            }
        }

        if (activePoolIndex == pools.size()) {
            if (activePoolBefore != activePoolIndex) {//if it just changed
                LOG(WARNING) << "no more backup pools available for PowType '" << powType
                             << "'. Waiting for pools to become available again.";
                if (pools.size() == 1) {
                    VLOG(0)
                        << "note: you can put multiple pools per PowType into the config file. additional pools will be used as backup.";
                }
            }
            else
                VLOG(0) << "still no backup pools available. Waiting for pools to become available again.";
        }
    }

    std::shared_ptr<Pool> PoolSwitcher::activePool() {
        if (activePoolIndex >= pools.size())
            return nullptr;
        auto pool = pools[activePoolIndex];
        RNR_EXPECTS(pool != nullptr);
        return pool;
    }

    unique_ptr<Work> PoolSwitcher::tryGetWorkImpl() {
        std::lock_guard<std::mutex> lock(mut);
        if (auto pool = activePool()) {
            return pool->tryGetWorkImpl();
        }

        VLOG(2) << "PoolSwitcher cannot provide work since there is no active pool";
        //wait for a short period of time to prevent busy waiting in the algorithms' loops
        //TODO: better solution would be to wait for a short time, and if a pool becomes available in that time period it can be used immediately and the function returns
        std::this_thread::sleep_for(std::chrono::seconds(1));
        return nullptr;
    }

    void PoolSwitcher::submitSolutionImpl(unique_ptr<WorkSolution> solution) {
        std::shared_ptr<const PoolJob> job = solution->tryGetJob();
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

    std::string PoolSwitcher::getName() const {
        return powType + "-PoolSwitcher";
    }

    size_t PoolSwitcher::poolCount() const {
        std::lock_guard<std::mutex> lock(mut);
        return pools.size();
    }

}
