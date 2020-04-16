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
            VLOG(6) << "alive-check thread started";
            periodicAliveCheck();
        });
    }

    PoolSwitcher::~PoolSwitcher() {
        {
            std::lock_guard<std::mutex> lock(mut);
            shutdown = true;
        }
        VLOG(6) << "shutting down poolswitcher " << powType << " thread";
        notifyOnShutdown.notify_all();
        periodicAliveCheckTask.wait();
        VLOG(6) << "sucessfully shut down poolswitcher " << powType << " thread";
    }

    void PoolSwitcher::periodicAliveCheck() {
        using namespace std::chrono;
        std::unique_lock<std::mutex> lock(mut);

        clock::duration usedCheckInterval = milliseconds(500);

        while(!shutdown) {
            float flt_sec = 0.001f * duration_cast<milliseconds>(usedCheckInterval).count();

            if (!pools.empty()) {
                VLOG(2) << "periodic pool connection status check (every " << flt_sec << "s)";
                aliveCheckAndMaybeSwitch();
            }
            else VLOG(2) << "no pools in poolswitcher, sleeping for " << flt_sec << "s";

            usedCheckInterval = activePool() ? checkInterval : milliseconds(500);

            //use condition vairable to wait, so the wait can be interrupted on shutdown
            notifyOnShutdown.wait_for(lock, usedCheckInterval, [this] {
                return (bool)shutdown;
            });
        }
    }

    void PoolSwitcher::aliveCheckAndMaybeSwitch() {
        //called while holding lock this->mut
        using namespace std::chrono;
        auto now = clock::now();
        auto durUntilDeclaredDeadSecs = duration_cast<seconds>(durUntilDeclaredDead).count();
        auto previousActivePoolIndex = activePoolIndex;

        //first lets write down the relevant info in this struct
        //so the logic below is better readable
        struct Info {
            size_t index = 0; //index in pools
            bool was_dead{}; //pool was dead during last check
            bool now_dead{}; //pool is now dead in this check
        };
        std::vector<Info> poolInfos{pools.size()};

        //fill the structs
        for (size_t i = 0; i < pools.size(); ++i) {
            poolInfos[i].index = i;
            poolInfos[i].was_dead = pools[i]->isDead();
            poolInfos[i].now_dead = now - pools[i]->getLastKnownAliveTime() > durUntilDeclaredDead;
        }

        //decide new active pool
        for (const Info &p : poolInfos) {
            if (!p.now_dead) {
                activePoolIndex = p.index;
                break; //first one that is not dead is chosen
            }
        }

        //declare/undeclare pools as dead
        for (const Info &p : poolInfos) {
            pools[p.index]->setDead(p.now_dead);
        }

        {//write descriptive logs (collapse this scope if needed)
            bool there_was_no_active_pool = previousActivePoolIndex >= pools.size();
            bool theres_no_active_pool = activePoolIndex >= pools.size();

            if (theres_no_active_pool && there_was_no_active_pool) {
                VLOG(0) << "still no backup pools available. Waiting for pools to become available again.";
            }

            if (activePoolIndex != previousActivePoolIndex && !theres_no_active_pool) {
                LOG(INFO) << "Pool #" << activePoolIndex << " (" << pools[activePoolIndex]->getName()
                          << ") chosen as new active pool";
            }

            for (const Info &p : poolInfos) {
                auto &pool = pools[p.index];

                if (!p.was_dead && p.now_dead) { //if just died
                    if (p.index == previousActivePoolIndex) { //pool that just died was the active pool
                        if (theres_no_active_pool) { //we now have no more backup
                            LOG(WARNING) << "no more backup pools available for PowType '" << powType
                                         << "'. Waiting for pools to become available again.";
                            if (pools.size() == 1) {
                                VLOG(0)
                                    << "note: you can put multiple pools per PowType into the config file. additional pools will be used as backup.";
                            }
                        } else { //we have a working backup pool
                            LOG(WARNING) << "active pool #" << p.index << "(" << pool->getName()
                                         << ") was inactive for "
                                         << durUntilDeclaredDeadSecs
                                         << " sec, switching to next backup pool";
                        }
                    } else { //pool that just died was not the active pool
                        LOG(INFO) << "currently unused backup pool #" << p.index << " (" << pool->getName()
                                  << ") was inactive for " << durUntilDeclaredDeadSecs << "s";
                    }
                }
            }
        } //end of log scope
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
