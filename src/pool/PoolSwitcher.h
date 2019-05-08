
#pragma once

#include "Pool.h"
#include <future>
#include <list>
#include <src/statistics/PoolRecords.h>
#include <src/application/Config.h>

namespace miner {

    //maintains connections to multiple WorkProviders (pools) in descending priority order.
    //tryGetWork always fetches work from the highest priority WorkProvider that does not
    //have a dead connection
    //a connection is considered dead if the WorkProvider (which extends StillAliveTrackable)
    //did not call its onStillAlive() for 'durUntilDeclaredDead' seconds
    //This condition being checked regularly on a separate thread every 'checkInterval' seconds
    class PoolSwitcher : public Pool {
    public:
        using clock = StillAliveTrackable::clock;

        explicit PoolSwitcher(clock::duration checkInterval = std::chrono::seconds(20),
                clock::duration durUntilDeclaredDead = std::chrono::seconds(60));

        ~PoolSwitcher() override;

        //the provided pool records will get connected to the total records of this PoolSwitcher
        Pool &push(unique_ptr<Pool> pool, unique_ptr<PoolRecords> records, PoolConstructionArgs args) {
            std::lock_guard<std::mutex> lock(mut);
            pools.emplace_back(PoolData{std::move(records), std::move(args), std::move(pool)});

            if (pools.size() == 1) {//if first pool was added, treat it as if it was alive (TODO: is this really desired behavior?)
                pools.back().pool->onStillAlive();
            }

            return *pools.back().pool;
        }

        optional<unique_ptr<WorkBase>> tryGetWork() override;

        void submitWork(unique_ptr<WorkResultBase>) override;

        size_t poolCount() const;

        uint64_t getPoolUid() const override;

        cstring_span getName() const override;

        struct ApiInfo {
            PoolRecords::Data totalRecords;
            struct Pool {
                std::string host;
                std::string port;
                std::string username;
                std::string password;
                PoolRecords::Data records;
            };
            std::vector<Pool> pools;
        };

        ApiInfo gatherApiInfo() const;

    private:
        //shutdown related variables
        bool shutdown = false;

        PoolRecords _totalRecords;

        mutable std::mutex mut;
        std::condition_variable notifyOnShutdown;

        //periodic checking
        void periodicAliveCheck();
        std::future<void> periodicAliveCheckTask;

        clock::duration checkInterval;
        clock::duration durUntilDeclaredDead;

        struct PoolData {
            unique_ptr<PoolRecords> records;
            PoolConstructionArgs args;
            unique_ptr<Pool> pool;
        };

        //TODO: replace unique_ptr with shared_ptr so that lock doesn't need to be held the entire time in tryGetWork();
        std::vector<PoolData> pools;
        size_t activePoolIndex = 0; //if > pools.size(), no pool is active

        optional_ref<Pool> activePool();

        void aliveCheckAndMaybeSwitch();

        uint64_t uid = createNewPoolUid();
    };

}