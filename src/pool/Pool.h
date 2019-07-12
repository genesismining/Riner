#pragma once

#include <src/common/WorkCommon.h>
#include <src/common/Pointers.h>
#include <src/common/Optional.h>
#include <src/common/Assert.h>
#include <src/util/Copy.h>
#include <src/pool/Work.h>
#include <src/common/StringSpan.h>
#include <string>
#include <list>
#include <atomic>
#include <chrono>

namespace miner {
    class PoolRecords;

    /**
     * @brief Args passed into every Pool subclass ctor
     * If you want to add args to every pool's ctor, add them to this struct instead
     *
     * don't confuse this with Config::Pool. PoolConstructionArgs may be used
     * to pass refs to other subsystems in the future (e.g. io_service?)
     */
    struct PoolConstructionArgs {
        std::string host;
        uint16_t port;
        std::string username;
        std::string password;
        PoolRecords &poolRecords;
    };

    /**
     * @brief pools extend this class to offer timestamp information of last incoming message to a PoolSwitcher
     * that way decisions can be made about which pool to prefer and which pool became inactive
     */
    class StillAliveTrackable {
    public:
        using clock = std::chrono::system_clock;

        StillAliveTrackable() = default;
        virtual ~StillAliveTrackable() = default;

        DELETE_COPY(StillAliveTrackable);

        /**
         * @return the timestamp of the most recent onStillAlive call
         * this method may be called from any thread
         */
        clock::time_point getLastKnownAliveTime();

        /**
         * manually change the timestamp that tracks the most recent onStillAlive call
         * this method may be called from any thread
         * @param time new value that overwrites the existing timestamp value
         */
        void setLastKnownAliveTime(clock::time_point time);

        /**
         * @brief call this if you just received an incoming message from the network, it will update the internal timestamp
         * this method may be called from any thread
         */
        void onStillAlive();

    private:
        std::atomic<clock::time_point> lastKnownAliveTime = {clock::time_point::min()};
    };

    /**
     * @brief common base class for all Pool protocol implementations
     * contains facilities for registering Pool Impls in a global list, so that the appropriate PoolImpl class can be
     * selected and instantiated based on a name string
     * when subclassing Pool, do not forget to add your PoolImpl class to the registry in 'Algorithm.cpp'
     */
    class Pool : public StillAliveTrackable {

        struct Entry {
            const std::string implName;
            const std::string algoName;
            ProtoEnum protoEnum;
            std::function<unique_ptr<Pool>(PoolConstructionArgs)> makeFunc;
        };

        static std::list<Entry> &getEntries() {
            static std::list<Entry> entries {};
            return entries;
        }

        static optional_ref<Entry> entryWithName(const std::string &implName);
        const Entry *info = nullptr;

    protected:
        /**
         * all pool uids get created through this call
         * @return a new pool uid that is never 0 for simpler debugging of uninitialized poolUids
         */
        static uint64_t createNewPoolUid();
        Pool() = default;

    public:
        virtual cstring_span getName() const = 0;

        /**
         * @return uid that makes it possible to identify which WorkSolution belongs to which pool in a PoolSwitcher
         */
        virtual uint64_t getPoolUid() const = 0;

        /**
         * @brief tryGetWork call as implemented by the Pool subclasses (aka PoolImpls)
         * this method is only supposed to be called by the templated tryGetWork<WorkT>() method below.
         * this method tries to return valid work as quickly as possible, but also blocks for a short amount of time to wait
         * for work to become available if it isn't already when the call is being made. After a short timeout period with
         * still no work available, nullopt is returned.
         * @return type erased valid unique_ptr or nullopt
         */
        virtual optional<unique_ptr<Work>> tryGetWork() = 0;

        /**
         * @brief submitWork call as implemented by the Pool subclasses (aka PoolImpls)
         * this method is only supposed to be called by the templated submitWork<WorkSolutionT>() method below.
         * this method takes the ownership of the given solution and starts its async submission, so that this
         * method can return quickly without blocking.
         * @param solution the solution to be submitted to the pool
         */
        virtual void submitWork(unique_ptr<WorkSolution> solution) = 0;

        /**
         * @brief obtain work from a pool
         * this method is supposed to be called from within an AlgoImpl. It is the intended way of obtaining work from a pool
         * this method tries to return valid work as quickly as possible, but also blocks for a short amount of time to wait
         * for work to become available if it isn't already when the call is being made. After a short timeout period with
         * still no work available, nullopt is returned.
         * This method is supposed to be called repeatedly in a loop in AlgoImpls, and will not cause busy waiting if
         * nullopt is returned repeatedly due to its internal timeout mechanism
         *
         * @tparam Work the Work subclass for a specific POWtype
         * @return a valid unique_ptr containing work or a nullopt optional
         */
        template<class WorkT>
        optional<unique_ptr<WorkT>> tryGetWork() {
            auto maybeWork = tryGetWork();
            if (maybeWork) {
                auto &work = maybeWork.value();
                MI_EXPECTS(work != nullptr && work->algorithmName == WorkT::getName());
                return static_unique_ptr_cast<Work>(std::move(maybeWork.value()));
            }
            return nullopt;
        }

        /**
         * @brief submit a found solution to the pool
         * this method is supposed to be called from within an AlgoImpl. It is the intended way of submitting solutions.
         * this method takes the ownership of the given solution and starts its async submission, so that this
         * method can return quickly without blocking.
         * AlgoImpls usually check the validity of a Solution before submitting through this call
         * @param solution the solution to be submitted to the pool
         */
        template<class WorkSolutionT>
        void submitWork(unique_ptr<WorkSolutionT> result) {
            MI_EXPECTS(result != nullptr && result->algorithmName == WorkSolutionT::getName());

            submitWork(static_unique_ptr_cast<WorkSolutionT>(std::move(result)));
        }

        /**
         * @return string name of the Pool subclass (aka PoolImpl) as it can be passed to makePool
         */
        inline std::string getImplName() const {
            return info->implName;
        }

        /**
         * get the POWtype that this Pool implements (e.g. in order to connect it with a fitting AlgoImpl, or group it
         * with Pools that support the same POWtype in a PoolSwitcher)
         * @return this pool's supported POWtype as a string
         */
        inline std::string getAlgoName() const {
            return info->algoName;
        }

        template<typename T>
        struct Registry {
            Registry(const std::string &name, const std::string &algoName, ProtoEnum protoEnum) noexcept {
                LOG(DEBUG) << "register Pool: " << name;

                getEntries().emplace_back(Entry{name, algoName, protoEnum});

                auto *entry = &getEntries().back();
                //create type erased creation function
                entry->makeFunc = [entry] (PoolConstructionArgs args) {
                    auto pool = std::make_unique<T>(std::move(args));
                    pool->info = entry;
                    return pool;
                };
            }

            Registry() = delete;
            DELETE_COPY_AND_MOVE(Registry);
        };

        //returns nullptr if no matching pool was found
        static unique_ptr<Pool> makePool(PoolConstructionArgs args, const std::string &poolImplName);
        static unique_ptr<Pool> makePool(PoolConstructionArgs args, const std::string &algoName, ProtoEnum);

        //returns implName
        static std::string getImplNameForAlgoTypeAndProtocol(const std::string &algoName, ProtoEnum);

        //returns empty string if implName doesn't match any pool impl
        static std::string getAlgoTypeForImplName(const std::string &implName);

        //returns kProtoCount if implName doesn't match any pool impl
        static ProtoEnum getProtocolForImplName(const std::string &implName);

        virtual ~Pool() = default;
        DELETE_COPY_AND_MOVE(Pool);
    };

}
