
#pragma once

#include "Pool.h"
#include <future>
#include <list>

namespace miner {

    //maintains connections to multiple WorkProviders (pools) in descending priority order.
    //tryGetWork always fetches work from the highest priority WorkProvider that does not
    //have a dead connection
    //a connection is considered dead if the WorkProvider (which extends StillAliveTrackable)
    //did not call its onStillAlive() for 'durUntilDeclaredDead' seconds
    //This condition being checked regularly on a separate thread every 'checkInterval' seconds
    class PoolSwitcher : public WorkProvider {
    public:
        using clock = StillAliveTrackable::clock;

        explicit PoolSwitcher(clock::duration checkInterval = std::chrono::seconds(20),
                clock::duration durUntilDeclaredDead = std::chrono::seconds(60));

        ~PoolSwitcher() override;

        template<class T>
        WorkProvider &emplace(PoolConstructionArgs args) {
            std::lock_guard<std::mutex> lock(mut);
            pools.emplace_back(std::make_unique<T>(std::move(args)));

            if (pools.size() == 1) //if first pool was added, treat it as if it was alive (TODO: is this really desired behavior?)
                pools.back()->onStillAlive();

            return *pools.back();
        }

        optional<unique_ptr<WorkBase>> tryGetWork() override;

        void submitWork(unique_ptr<WorkResultBase>) override;

        uint64_t getPoolUid() const override;

        cstring_span getName() const override;

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

        void aliveCheckAndMaybeSwitch();

        uint64_t uid = createNewPoolUid();
    };

}