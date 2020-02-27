#pragma once

#include <src/common/Pointers.h>
#include <src/common/Optional.h>
#include <src/common/Assert.h>
#include <src/util/Copy.h>
#include <src/pool/Work.h>
#include <src/statistics/PoolRecords.h>
#include <string>
#include <list>
#include <atomic>
#include <chrono>

namespace riner {

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
         * @return the timestamp of when this base class was initialized in an object
         * this method may be called from any thread
         */
        clock::time_point getObjectCreationTime();

        /**
         * if the lastKnownAliveTime is considered to long ago, this function is called to
         * declare this object 'dead' (and call onDeclaredDead internally).
         */
        void declareDead();

        /**
         * @return the timestamp of the most recent onDeclaredDead call
         * this method may be called from any thread
         */
        clock::time_point getLatestDeclaredDeadTime();

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

    protected:
        /**
         * gets called when the lastKnownAliveTime is considered to long ago, so the
         * object is considered 'dead'. The object should try to reestablish the alive status
         * so it can be used again. (for a pool this means, disconnect and try to
         * reconnect to the pool)
         */
        virtual void onDeclaredDead() = 0;

    private:
        const std::atomic<clock::time_point> objectCreationTime = {clock::now()};
        std::atomic<clock::time_point> lastKnownAliveTime = {clock::time_point::min()};
        std::atomic<clock::time_point> latestDeclaredDeadTime = {clock::time_point::min()};
    };

    /**
     * @brief common base class for all Pool protocol implementations
     * when subclassing Pool, do not forget to add your PoolImpl class to the registry in 'Registry.cpp'
     */
    class Pool : public StillAliveTrackable {
    protected:

        explicit Pool(PoolConstructionArgs args);
        std::weak_ptr<Pool> _this; //assigned in Registry's initFunc lambda, which creates the shared_ptr, which is ok because even if the Pool ctor starts iothreads which use _this before it is assigned, the mechanisms that use it are nullptr safe.
        const char *_poolImplName = ""; //assigned in Registry...
        const char *_powType = ""; //assigned in Registry...
        std::atomic_bool connected {false};
        PoolRecords records;

    public:
        Pool() = delete;

        const PoolConstructionArgs constructionArgs;

        static void postInit(std::shared_ptr<Pool> w, const char *poolImplName, const char *powType) { //inits private/protected base class members without requiring the user to pass them through
            w->_this = w;
            w->_poolImplName = poolImplName;
            w->_powType = powType;
        }
        static uint64_t generateUid();

        const uint64_t poolUid = generateUid();

        inline auto readRecords() const {
            return records.read();
        }

        inline void addRecordsListener(PoolRecords &parent) {
            records.addListener(parent);
        }

        virtual std::string getName() const {
            return constructionArgs.host + ":" + std::to_string(constructionArgs.port);
        }

        virtual bool isExpiredJob(const PoolJob &job) {
            return true;
        }

        virtual void onDeclaredDead() override = 0; //override this function to handle trying to reconnect after being declared dead

        /**
         * @brief tryGetWorkImpl call as implemented by the Pool subclasses (aka PoolImpls)
         *
         * this method is only supposed to be called by the templated tryGetWork<WorkT>() method below.
         * this method tries to return valid work as quickly as possible, but also blocks for a short amount of time to wait
         * for work to become available if it isn't already when the call is being made. After a short timeout period with
         * still no work available, nullptr is returned.
         * @return type erased valid unique_ptr or nullptr
         */
        virtual unique_ptr<Work> tryGetWorkImpl() = 0;

        /**
         * @brief submitSolutionImpl call as implemented by the Pool subclasses (aka PoolImpls)
         *
         * this method is only supposed to be called by the templated submitSolution<WorkSolutionT>() method below.
         * this method takes the ownership of the given solution and starts its async submission, so that this
         * method can return quickly without blocking.
         * @param solution the solution to be submitted to the pool
         */
        virtual void submitSolutionImpl(unique_ptr<WorkSolution> solution) = 0;

        /**
         * @brief obtain work from a pool
         *
         * this method is supposed to be called from within an AlgoImpl. It is the intended way of obtaining work from a pool
         * this method tries to return valid work as quickly as possible, but also blocks for a short amount of time to wait
         * for work to become available if it isn't already when the call is being made. After a short timeout period with
         * still no work available, nullptr is returned.
         * This method is supposed to be called repeatedly in a loop in AlgoImpls, and will not cause busy waiting if
         * nullopt is returned repeatedly due to its internal timeout mechanism
         *
         * @tparam WorkT the Work subclass for a specific POWtype
         * @return a valid unique_ptr containing work or a nullptr
         */
        template<class WorkT>
        unique_ptr<WorkT> tryGetWork() {
            auto work = tryGetWorkImpl();
            if (work) {
                RNR_EXPECTS(work->powType == WorkT::getPowType());
                return static_unique_ptr_cast<WorkT>(std::move(work));
            }
            return nullptr;
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
        void submitSolution(unique_ptr<WorkSolutionT> result) {
            RNR_EXPECTS(result != nullptr && result->powType == WorkSolutionT::getPowType());

            submitSolutionImpl(static_unique_ptr_cast<WorkSolutionT>(std::move(result)));
        }

        /**
         * @return whether a connection is established
         */
        inline bool isConnected() const {
            return connected;
        }

        /**
         * @return string name of the Pool subclass (aka PoolImpl) as it can be passed to makePool
         */
        inline std::string getPoolImplName() const {
            return _poolImplName;
        }

        /**
         * get the POWtype that this Pool implements (e.g. in order to connect it with a fitting AlgoImpl, or group it
         * with Pools that support the same POWtype in a PoolSwitcher)
         * @return this pool's supported POWtype as a string
         */
        inline std::string getPowType() const {
            return _powType;
        }

        ~Pool() override = default;

        DELETE_COPY_AND_MOVE(Pool);
    };

}
