//
//

#include "PoolSwitcher.h"
#include "WorkQueue.h"
#include <src/common/Assert.h>
#include <src/algorithm/Algorithm.h>

namespace riner {


    PoolSwitcher::PoolSwitcher(std::string powType, clock::duration checkInterval, clock::duration durUntilDeclaredDead)
            : Pool(PoolConstructionArgs{})
            , checkInterval(checkInterval)
            , durUntilDeclaredDead(durUntilDeclaredDead) {

        RNR_EXPECTS(!powType.empty());
        _powType = powType;
        _poolImplName = powType + "-PoolSwitcher";

        onStateChange = std::make_shared<std::condition_variable>();
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
        VLOG(6) << "shutting down poolswitcher " << _powType << " thread";
        onStateChange->notify_all();
        periodicAliveCheckTask.wait();
        VLOG(6) << "sucessfully shut down poolswitcher " << _powType << " thread";
    }

    void PoolSwitcher::periodicAliveCheck() {
        auto flt_sec = std::chrono::duration<float>(checkInterval).count();
        std::unique_lock<std::mutex> lock(mut);

        while (!shutdown) {

            bool was_active = active_pool && (active_pool->isConnected() && !active_pool->isDisabled());
            auto prevActivePoolIndex = activePoolIndex;
            if (!pools.empty()) {
                VLOG(2) << "periodic pool connection status check (every " << flt_sec << "s)";
                aliveCheckAndMaybeSwitch();
            }
            else {
                VLOG(2) << "no pools in poolswitcher, sleeping for " << flt_sec << "s";
            }
            //if pool switcher selects new pool, then the new pool should be active
            was_active |= activePoolIndex != prevActivePoolIndex;
            lock.unlock();
            onStateChange->notify_all();
            lock.lock();

            //use condition vairable to wait, so the wait can be interrupted on shutdown
            onStateChange->wait_for(lock, checkInterval, [this, was_active] {
                bool wakeup = shutdown;
                if (wakeup) {
                }
                else if (was_active && active_pool) {
                    wakeup = active_pool->isDisabled() || !active_pool->isConnected();
                }
                else {
                    for (const auto pool : pools) {
                        if (wakeup = pool->isConnected() && !pool->isDisabled()) {
                            break;
                        }
                    }
                }
                return wakeup;
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
            bool disabled{};
            bool connected{};
        };
        std::vector<Info> poolInfos{pools.size()};

        //fill the structs
        for (size_t i = 0; i < pools.size(); ++i) {
            poolInfos[i].index = i;
            poolInfos[i].was_dead = pools[i]->isDead();
            poolInfos[i].now_dead = now - pools[i]->getLastKnownAliveTime() > durUntilDeclaredDead;
            poolInfos[i].disabled = pools[i]->isDisabled();
            poolInfos[i].connected = pools[i]->isConnected();
        }

        //decide new active pool
        for (const Info &p : poolInfos) {
            if (p.connected && !p.now_dead && !p.disabled) {
                bool is_another_pool = active_pool && active_pool != pools[p.index];
                active_pool = pools[p.index];
                if (is_another_pool) {
                    pools[activePoolIndex]->expireJobs();
                }
                activePoolIndex = p.index;
                break; //first one that is not dead is chosen
            }
        }

        //declare/undeclare pools as dead, close connections of disabled pools
        for (const Info &p : poolInfos) {
            pools[p.index]->setDead(p.now_dead);
            if (p.disabled && p.connected) {
                LOG(INFO) << "pool disabled. kill connection.";
                pools[p.index]->onDeclaredDead();
            }
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
                            LOG(WARNING) << "no more backup pools available for PowType '" << _powType
                                         << "'. Waiting for pools to become available again.";
                            if (pools.size() == 1) {
                                VLOG(0)
                                    << "note: you can put multiple pools per PowType into the config file. additional pools will be used as backup.";
                            }
                        }
                        else { //we have a working backup pool
                            LOG(WARNING) << "active pool #" << p.index << "(" << pool->getName()
                                         << ") was inactive for "
                                         << durUntilDeclaredDeadSecs
                                         << " sec, switching to next backup pool";
                        }
                    }
                    else if (p.connected) { //pool that just died was not the active pool
                        LOG(INFO) << "currently unused backup pool #" << p.index << " (" << pool->getName()
                                  << ") was inactive for " << durUntilDeclaredDeadSecs << "s";
                    }
                }
            }
        } //end of log scope
    }

    unique_ptr<Work> PoolSwitcher::tryGetWorkImpl() {
        if (auto pool = active_pool) {
            return pool->tryGetWorkImpl();
        }

        VLOG(2) << "PoolSwitcher cannot provide work since there is no active pool";
        //wait for event to prevent busy waiting in the algorithms' loops
        std::unique_lock<std::mutex> lock(mut);
        onStateChange->wait(lock, [this] () {
            return active_pool || shutdown;
        });
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

        if (pool->isConnected()) {

            if (auto _active_pool = active_pool) {
                activePoolUid = _active_pool->poolUid;
            }

            sameUid = activePoolUid == solutionPoolUid;
            if (!sameUid) {
                LOG(INFO) << "solution will be submitted to non-active pool (uid " << solutionPoolUid << ") and not to current pool (uid " << activePoolUid << ")";
            }
            return pool->submitSolutionImpl(std::move(solution));
        }
        LOG(INFO) << "solution could not be submitted, since there is no active pool";
    }

    std::string PoolSwitcher::getName() const {
        return _powType + "-PoolSwitcher";
    }

    size_t PoolSwitcher::poolCount() const {
        std::lock_guard<std::mutex> lock(mut);
        return pools.size();
    }

}
