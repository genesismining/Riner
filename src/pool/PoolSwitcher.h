
#pragma once

#include "Pool.h"
#include <future>
#include <list>

namespace miner {

    class PoolSwitcher : public WorkProvider {
    public:
        using clock = StillAliveTrackable::clock;

        explicit PoolSwitcher(clock::duration checkInterval = std::chrono::seconds(30),
                clock::duration durUntilDeclaredDead  = std::chrono::seconds(30));

        ~PoolSwitcher() override;

        template<class T>
        WorkProvider &emplace(PoolConstructionArgs args) {
            std::lock_guard<std::mutex> lock(mut);
            pools.emplace_back(std::make_unique<T>(std::move(args)));
            return *pools.back();
        }

        optional<unique_ptr<WorkBase>> tryGetWork() override;

        void submitWork(unique_ptr<WorkResultBase>) override;

        uint64_t getPoolUid() const override;

    private:
        //shutdown related variables
        bool shutdown = false;
        std::mutex mut;
        std::condition_variable notifyOnShutdown;

        //periodic checking
        void periodicAliveCheck();
        std::future<void> periodicAliveCheckTask;

        clock::duration checkInterval;
        clock::duration durUntilDeclaredDead;

        std::vector<unique_ptr<WorkProvider>> pools;
        size_t activePoolIndex = 0; //if > pools.size(), no pool is active

        optional_ref<WorkProvider> activePool();

        void aliveCheck();

        int64_t uid = createNewPoolUid();
    };

}