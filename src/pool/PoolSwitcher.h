
#pragma once

#include "Pool.h"
#include <future>
#include <list>
#include <src/config/Config.h>

namespace miner {

    //maintains connections to multiple Pools in descending priority order.
    //tryGetWork always fetches work from the highest priority Pool that does not
    //have a dead connection
    //a connection is considered dead if the Pool (which extends StillAliveTrackable)
    //did not call its onStillAlive() for 'durUntilDeclaredDead' seconds
    //This condition being checked regularly on a separate thread every 'checkInterval' seconds
    class PoolSwitcher : public Pool {
    public:
        using clock = StillAliveTrackable::clock;

        explicit PoolSwitcher(std::string powType,
                clock::duration checkInterval = std::chrono::seconds(20),
                clock::duration durUntilDeclaredDead = std::chrono::seconds(60));

        ~PoolSwitcher() override;

        //the provided pool records will get connected to the total records of this PoolSwitcher
        std::shared_ptr<Pool> tryAddPool(const PoolConstructionArgs &args, const std::string &poolImplName) {
            std::shared_ptr<Pool> pool = Pool::makePool(args, poolImplName);
            MI_EXPECTS(pool != nullptr);

            pool->addRecordsListener(records);
            std::lock_guard<std::mutex> lock(mut);
            pools.emplace_back(pool);

            if (pools.size() == 1) {//if first pool was added, treat it as if it was alive (TODO: is this really desired behavior?)
                pools.back()->onStillAlive();
            }

            return pool;
        }

        unique_ptr <Work> tryGetWorkImpl() override;

        void submitSolutionImpl(unique_ptr<WorkSolution>) override;

        void onDeclaredDead() override {};//poolswitcher should no longer subclass Pool

        size_t poolCount() const;

        std::string getName() const override;

        std::vector<std::shared_ptr<const Pool>> getPoolsData() const {
            std::lock_guard<std::mutex> lock(mut);
            std::vector<std::shared_ptr<const Pool>> constPools(pools.size());
            for (size_t i = 0; i < pools.size(); i++) {
                constPools[i] = pools[i];
            }
            return constPools;
        }

    private:
        //shutdown related variables
        bool shutdown = false;

        mutable std::mutex mut;
        std::condition_variable notifyOnShutdown;

        //periodic checking
        void periodicAliveCheck();
        std::future<void> periodicAliveCheckTask;

        const std::string powType;
        clock::duration checkInterval;
        clock::duration durUntilDeclaredDead;

        std::vector<shared_ptr<Pool>> pools;
        size_t activePoolIndex = 0; //if > pools.size(), no pool is active

        std::shared_ptr<Pool> activePool();

        void aliveCheckAndMaybeSwitch();
    };

}