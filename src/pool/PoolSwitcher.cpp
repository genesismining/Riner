//
//

#include "PoolSwitcher.h"
#include <src/common/Assert.h>

namespace miner {


    PoolSwitcher::PoolSwitcher(clock::duration checkInterval, clock::duration durUntilDeclaredDead)
    : checkInterval(checkInterval)
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

            LOG(INFO) << "check for dead pools";
            aliveCheckAndMaybeSwitch();

            //use condition vairable to wait, so the wait can be interrupted on shutdown
            notifyOnShutdown.wait_for(lock, checkInterval, [this] {
                return shutdown;
            });
        }
    }

    void PoolSwitcher::aliveCheckAndMaybeSwitch() {
        auto now = clock::now();

        for (size_t i = 0; i < pools.size(); ++i) {
            auto &pool = pools[i];

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

    optional_ref<WorkProvider> PoolSwitcher::activePool() {
        if (activePoolIndex >= pools.size())
            return nullopt;
        auto &pool = pools[activePoolIndex];
        MI_EXPECTS(pool != nullptr);
        return type_safe::opt_ref(*pool);
    }

    optional<unique_ptr<WorkBase>> PoolSwitcher::tryGetWork() {
        std::lock_guard<std::mutex> lock(mut);
        auto pool = activePool();
        if (pool) {
            LOG(INFO) << "getting work from " << gsl::to_string(pool.value().getName());
            return pool.value().tryGetWork();
        }

        //TODO: make this call wait for some time before it returns nullopt
        // like the other tryGetWork implementations, so that there's no
        // busy waiting happening on the algorithm side
        LOG(INFO) << "PoolSwitcher cannot provide work since there is no active pool";
        std::this_thread::sleep_for(std::chrono::seconds(1));
        return nullopt;
    }

    void PoolSwitcher::submitWork(unique_ptr<WorkResultBase> result) {
        auto resultPoolUid = result->getProtocolData().lock()->getPoolUid();
        auto activePoolUid = std::numeric_limits<decltype(resultPoolUid)>::max();
        bool sameUid = true;

        {
            std::lock_guard<std::mutex> lock(mut);

            if (auto pool = activePool()) {

                activePoolUid = pool.value().getPoolUid();

                sameUid = activePoolUid == resultPoolUid;
                if (sameUid) {
                    return pool.value().submitWork(std::move(result));
                }
            }
        } //unlock

        if (sameUid)
            LOG(INFO) << "solution could not be submitted, since there is no active pool";
        else
            LOG(INFO) << "solution belongs to another pool (uid " << resultPoolUid << ") and will not be submitted to current pool (uid " << activePoolUid << ")";
    }

    uint64_t PoolSwitcher::getPoolUid() const {
        return uid;
    }

    cstring_span PoolSwitcher::getName() const {
        return "PoolSwitcher at " + std::to_string(uintptr_t(this));
    }

}