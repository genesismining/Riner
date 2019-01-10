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

            aliveCheck();

            //use condition vairable to wait, so the wait can be interrupted on shutdown
            notifyOnShutdown.wait_for(lock, checkInterval, [this] {
                return shutdown;
            });
        }
    }

    void PoolSwitcher::aliveCheck() {
        auto now = clock::now();

        for (size_t i = 0; i < pools.size(); ++i) {
            auto &pool = pools[i];

            auto lastKnownAliveTime = pool->getLastKnownAliveTime();

            if (i <= activePoolIndex) {
                if (now - lastKnownAliveTime > durUntilDeclaredDead) {
                    if (i == activePoolIndex) {
                        ++activePoolIndex;

                        LOG(INFO) << "Pool #" << i << " is inactive, trying next backup pool";
                    }
                }
                else {
                    activePoolIndex = i;

                    LOG(INFO) << "Pool #" << i << " chosen as new active pool";
                }
            }
        }

        if (activePoolIndex == pools.size()) {
            LOG(INFO) << "no more backup pools available. Waiting for pools to become available again.";
        }
    }

    optional_ref<WorkProvider> PoolSwitcher::activePool() {
        if (activePoolIndex > pools.size())
            return nullopt;
        auto &pool = pools[activePoolIndex];
        MI_EXPECTS(pool != nullptr);
        return type_safe::opt_ref(*pool);
    }

    optional<unique_ptr<WorkBase>> PoolSwitcher::tryGetWork() {
        std::lock_guard<std::mutex> lock(mut);
        auto pool = activePool();
        if (pool)
            return pool.value().tryGetWork();

        //TODO: make this call wait for some time before it returns nullopt
        // like the other tryGetWork implementations, so that there's no
        // busy waiting happening on the algorithm side
        std::this_thread::sleep_for(std::chrono::seconds(1));
        return nullopt;
    }

    void PoolSwitcher::submitWork(unique_ptr<WorkResultBase> result) {
        auto resultPoolUid = result->getProtocolData().lock()->getPoolUid();
        bool sameUid = true;

        {
            std::lock_guard<std::mutex> lock(mut);

            if (auto pool = activePool()) {

                sameUid = pool.value().getPoolUid() == resultPoolUid;
                if (sameUid) {
                    return pool.value().submitWork(std::move(result));
                }
            }
        } //unlock

        if (sameUid)
            LOG(INFO) << "solution could not be submitted, since there is no active pool";
        else
            LOG(INFO) << "solution belongs to another pool and will not be submitted";
    }

    uint64_t PoolSwitcher::getPoolUid() const {
        return uid;
    }

}