
#pragma once

#include "Pool.h"
#include <future>
#include <list>
#include <src/config/Config.h>
#include <src/application/Registry.h>
#include <atomic>

namespace riner {

    /**
     * maintains connections to multiple Pools as written down in the Config in descending priority order.
     * `tryGetWork` always fetches work from the highest priority Pool that does not
     * have a dead connection.
     * A connection is considered dead if the Pool (which extends StillAliveTrackable)
     * did not call its `onStillAlive()` for `PoolSwitcher::durUntilDeclaredDead` seconds
     * This condition is being checked regularly on a separate thread every 'checkInterval' seconds by this class
     */
    class PoolSwitcher : public Pool {
    public:
        using clock = StillAliveTrackable::clock;

        /**
         * creates a PoolSwitcher for a given PowType
         * param powType the PowType of all pools that will ever be added to this pool switcher
         * param checkInterval the interval in which the alive status (see `StillAliveTrackable`) of pools is checked
         * param durUntilDeclaredDead if the checking thread notices that the last life sign of a pool was longer ago than this duration, the pool will be declared dead, and the next alive pool will be chosen as active pool.
         */
        explicit PoolSwitcher(std::string powType,
                clock::duration checkInterval = std::chrono::seconds(20),
                clock::duration durUntilDeclaredDead = std::chrono::seconds(60));

        ~PoolSwitcher() override;

        /**
         * Tries to construct a pool (may fail if the poolImplName doesn't exist in `Registry`)
         * the new pool's `PoolRecords` will get connected to the total records of this PoolSwitcher
         */
        std::shared_ptr<Pool> tryAddPool(const PoolConstructionArgs &args, const char *poolImplName, const Registry &registry = Registry{}) {
            std::shared_ptr<Pool> pool = registry.makePool(poolImplName, args);
            RNR_EXPECTS(pool != nullptr);

            pool->addRecordsListener(records);
            pool->setOnStateChangeCv(onStateChange);
            _pools.lock()->emplace_back(pool);
            notifyOnPoolsChange();

            return pool;
        }

        /**
         * redirects the tryGetWork call to the active pool.
         * return nullptr if either the active pool does not have work, or there is no active pool. Valid work otherwise, which can be downcast to the specific work type (e.g. WorkEthash for powType "ethash")
         */
        unique_ptr <Work> tryGetWorkImpl() override;

        /**
         * redirects the submitSolution call to the active pool. See implementation for more details
         */
        void submitSolutionImpl(unique_ptr<WorkSolution>) override;

        /**
         * obsolete
         * poolswitcher should be no longer subclass of Pool
         */
        void onDeclaredDead() override {}
        bool isExpiredJob(const PoolJob &job) override {return true; }
        void expireJobs() override {}
        void clearJobs() override {}
        /***/

        /**
         * amount of pools added to this pool switcher
         */
        size_t poolCount() const;

        /**
         * return const references to the pools with shared ownership. useful for introspection of
         * pool data (e.g. by ApiServer)
         */
        std::vector<std::shared_ptr<const Pool>> getPoolsData() const {
            auto pools_lock_guard = _pools.readLock();
            const auto &pools = *pools_lock_guard;
            std::vector<std::shared_ptr<const Pool>> constPools(pools.size());
            for (size_t i = 0; i < pools.size(); i++) {
                constPools[i] = pools[i];
            }
            return constPools;
        }

    private:
        //shutdown related variables
        std::atomic_bool shutdown {false};

        mutable std::mutex mut;

        //periodic checking
        void periodicAliveCheck();
        std::future<void> periodicAliveCheckTask;

        clock::duration checkInterval;
        clock::duration durUntilDeclaredDead;

        SharedLockGuarded<std::vector<shared_ptr<Pool>>> _pools;
        std::atomic<bool> pools_changed{false};

        /**
         * after changes of _pools this method shall be called, so that the PoolSwitcher can check pools immediately
         */
        inline void notifyOnPoolsChange() {
            pools_changed = true;
            onStateChange->notify_all();
        }

        struct {
        private:
            std::shared_ptr<Pool> pool;

        public:
            inline std::shared_ptr<Pool> get() const {
                return std::atomic_load(&pool);
            }

            inline void set(std::shared_ptr<Pool> new_pool) {
                std::atomic_store(&pool, new_pool);
            }
        } active_pool;

        /**
         * check which pools are still alive and if the active pool is no longer alive, switch active pool.
         * called by the periodicAliveCheckTask thread
         */
        size_t aliveCheckAndMaybeSwitch(size_t activePoolIndex);
    };

}
