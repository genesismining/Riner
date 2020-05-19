#pragma once

#include <src/common/Pointers.h>
#include <src/common/Optional.h>
#include <src/common/Assert.h>
#include <src/network/SslDesc.h>
#include <src/util/Copy.h>
#include <src/pool/Work.h>
#include <src/common/Chrono.h>
#include <src/statistics/PoolRecords.h>
#include <string>
#include <list>
#include <atomic>

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
        SslDesc sslDesc;
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
         * If subsequent OnStillAlive messages have been noticed, the dead state will be revoked
         */
        void setDead(bool isDead);

        /**
         * if the object's alive times suggest it has reawakened after it was declared dead
         * this function is called
         */
        bool isDead() const;

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
        std::atomic_bool _isDead {true}; //starts in dead state
        std::atomic<clock::time_point> lastKnownAliveTime     = {clock::now() - std::chrono::hours(24 * 365)};
        std::atomic<clock::time_point> latestDeclaredDeadTime = {clock::now() - std::chrono::hours(24 * 365)}; //lets set the default value to one year ago, since clock::time_point::min() results in integer overflow bugs
    };

    /**
     * @brief common base class for all Pool protocol implementations
     * when subclassing Pool, do not forget to add your PoolImpl class to the registry in 'Registry.cpp'
     */
    class Pool : public StillAliveTrackable {

    private:
        std::atomic_bool _active {true};
        std::atomic_bool _connected {false};
        std::atomic_bool _disabled {false};

    protected:

        explicit Pool(PoolConstructionArgs args);
        std::weak_ptr<Pool> _this; //assigned in Registry's initFunc lambda, which creates the shared_ptr, which is ok because even if the Pool ctor starts iothreads which use _this before it is assigned, the mechanisms that use it are nullptr safe.
        std::string _poolImplName = ""; //assigned in Registry...
        std::string _powType = ""; //assigned in Registry...
        PoolRecords records;
        std::shared_ptr<std::condition_variable> onStateChange;

        /**
         * @brief writes the connected flag of the pool, which indicates whether a connection is established and whether it received a job yet
         * @param connected new value of connected flag
         */
        void setConnected(bool connected);

         /**
         * @brief changes the disabled status of the pool
         * If the disabled flag is set, then no further connection attempt to the pool will be made
         * @param disabled new value of disabled flag
         */
        void setDisabled(bool disabled);

    public:
        Pool() = delete;

        //the arguments from construction stay accessible here throughout the lifetime of the object
        const PoolConstructionArgs constructionArgs;

        /**
         * inits private/protected base class members without requiring the user to pass them through. called by the poolswitcher during initialization
         * @param w a shared pointer owning *this, which gets stored in this->_this
         * @param poolImplName the implementation name according to Registry
         * @param powType the powtype string according to Registry
         */
        static void postInit(std::shared_ptr<Pool> w, const std::string &poolImplName, const std::string &powType);
        
        /**
         * simple uid creation function for pools (thread safe)
         */
        static uint64_t generateUid();

        const uint64_t poolUid = generateUid();

        /**
         * get a copy of current PoolRecords data state
         */
        inline auto readRecords() const {
            return records.read();
        }

        /**
         * adds a listener to the PoolRecords associated with this pool, which can aggregate stat changes
         */
        inline void addRecordsListener(PoolRecords &parent) {
            records.addListener(parent);
        }

        /**
         * sets the onStateChange condition variable, so that the calling thread can be notified later
         */
        inline void setOnStateChangeCv(std::shared_ptr<std::condition_variable> cv) {
            onStateChange = cv;
        }

        /**
         * @return host:port in a printable way
         */
        virtual std::string getName() const {
            return constructionArgs.host + ":" + std::to_string(constructionArgs.port);
        }

        /**
         * @return whether the pool job is considered expired. Most implementations forward this call to the WorkQueue's `WorkQueue::isExpiredJob()` method, which returns false as soon as a newer job was pushed.
         */
        virtual bool isExpiredJob(const PoolJob &job) = 0;

        /**
        * on pool switch this method is called by the PoolSwitcher, so that all Work from this pool is marked as expired
        * and the GPUs request new work after this
        */
        virtual void expireJobs() = 0;

        /**
         * clears the job queue and therefore all Work and WorkSolution from this pool is invalidated.
         */
        virtual void clearJobs() = 0;

        /**
         * @brief This method is called by the PoolSwitcher and it changes the active status of the pool
         * As long as the active flag is cleared no connection attempt to the pool shall be made and
         * when the flag is cleared, then the current connection is closed immediately
         * @param active new value of active flag
         */
        void setActive(bool active);

        /**
         * The pool switcher will declare pools dead if they didn't respond for a certain amount of time.
         * PoolImpls are supposed to call `StillAliveTrackable::onStillAlive` every time they receive a
         * message from the pool. The StillAliveTrackable interface is used to figure out whether to
         * declare a pool dead. Once it is declared dead the PoolImpl can react by overloading this
         * function and trying to disconnect and reconnect to the pool.
         */
        virtual void onDeclaredDead() override = 0;

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
         * @return whether the pool shall try to establish a connection
         */
        inline bool isActive() const {
            return _active;
        }

        /**
         * @return whether a connection is established and whether it received a job yet
         */
        inline bool isConnected() const {
            return _connected;
        }

        /**
         * @return whether the pool is disabled, e.g. because of wrong pool credentials
         */
        inline bool isDisabled() const {
            return _disabled;
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
