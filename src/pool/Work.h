
#pragma once

#include <src/common/Pointers.h>
#include <src/common/Optional.h>
#include <atomic>
#include <string>

namespace miner {

    class Work;
    class Pool;
    class WorkQueue;
    class LazyWorkQueue;


    /**
     * @brief Subclasses save all necessary pool and protocol related data
     */
    struct PoolJob {
        std::weak_ptr<Pool> pool;
        int64_t id;

        bool expired() const;
        bool valid() const;

        virtual std::unique_ptr<Work> makeWork() = 0;
        virtual ~PoolJob() = default;

        PoolJob() = delete;
        PoolJob(std::weak_ptr<Pool> pool);
    };


    /** @brief Solution counterpart of the Work class.
     * Every Work subclass is expected to have a corresponding WorkSolution subclass for a given POWtype.
     * WorkSolutions should be created via a Work's makeWorkSolution<T>() method, filled with the actual proof of work data and then submitted to the Pool
     */
    class WorkSolution {
        std::weak_ptr<const PoolJob> job;

    protected:
        WorkSolution(const Work &work);

    public:
        template<class T>
        std::shared_ptr<const T> getCastedJob() const {
            static_assert(std::is_base_of<PoolJob, T>::value, "");
            return std::static_pointer_cast<const T>(job.lock()); //T::getPowType() == powType ? std::static_pointer_cast<const T>(job.lock()) : nullptr;
        }

        shared_ptr<const PoolJob> getJob() const {
            return job.lock();
        }

        const std::string powType;

        virtual ~WorkSolution() = default;

        template<class T>
        T *downCast() {
            static_assert(std::is_base_of<WorkSolution, T>::value, "");
            return T::getPowType() == powType ? static_cast<T*>(this) : nullptr;
        }

        template<class T>
        const T *downCast() const {
            static_assert(std::is_base_of<WorkSolution, T>::value, "");
            return T::getPowType() == powType ? static_cast<const T*>(this) : nullptr;
        }

        /**
         * @brief checks whether solution belongs to the most recent work available to the pool protocol.
         * Usually, an expired solution should be accepted by the pool protocol implementation, too.
         *
         * @return whether the solution has been marked expired by the pool protocol implementation
         */
        bool expired() const { //thread safe
            bool expired = true;
            if (auto sharedPtr = job.lock())
                expired = sharedPtr->expired();
            return expired;
        }

        /**
         * @brief checks whether solution would be still accepted by the pool protocol.
         * if valid() returns false, then the solution will be rejected by the pool protocol implementation.
         *
         * @return whether solution would be accepted by the pool protocol implementation
         */
        bool valid() const {
            //checks whether associated shared_ptrs are still alive
            bool valid = false;
            if (auto sharedPtr = job.lock())
                valid = sharedPtr->valid();
            return valid;
        }
    };


    /** @brief base class for all classes representing a unit of work of a POWtype as provided by the pool protocol (e.g. see WorkEthash)
     * this unit of work does not necessarily correspond to e.g. a stratum job, it may be a work representation of smaller granularity.
     * The pool protocol implementation is incentivized to make this work of appropriate size (read: amount of work, not byte size) so that minimal overhead is
     * required on the AlgoImpl side to make use of it.
     * Data that is specific to Pool protocol implementations may not be added to a Work subclass, but rather to a WorkProtocolData subclass
     */
    class Work {

        std::weak_ptr<const PoolJob> job;
        friend class WorkQueue;
        friend class LazyWorkQueue;
        friend class WorkSolution;

    protected:
        Work(const std::string &powType);

    public:
        const std::string powType;
        virtual ~Work() = default;

        template<class T>
        T *downCast() {
            static_assert(std::is_base_of<Work, T>::value, "");
            return T::getPowType() == powType ? static_cast<T*>(this) : nullptr;
        }

        template<class T>
        const T *downCast() const {
            static_assert(std::is_base_of<Work, T>::value, "");
            return T::getPowType() == powType ? static_cast<const T*>(this) : nullptr;
        }

        /**
         * @brief checks whether work is the most recent work available to the pool protocol.
         * expired() can be used as hint for algorithms whether new work shall be requested from the pool.
         * Upon expiration it is expected (but not necessary) that an algorithm stops working on this work,
         * if fresh work can be acquired from the pool.
         *
         * @return whether the work has been marked expired by the pool protocol implementation
         */
        bool expired() const { //thread safe
            bool expired = true;
            if (auto sharedPtr = job.lock())
                expired = sharedPtr->expired();
            return expired;
        }

        /**
         * @brief checks whether solutions of this work would be still accepted by the pool protocol.
         * if valid() returns false, then it does not make sense to continue finding solutions for this work.
         *
         * @return whether a solution for this work would be accepted by the pool protocol implementation
         */
        bool valid() const {
            //checks whether associated shared_ptrs are still alive
            bool valid = false;
            if (auto sharedPtr = job.lock())
                valid = sharedPtr->valid();
            return valid;
        }


        /**
         * @brief creates a solution object for this work object.
         * creates a solution object which can be filled with the algorithm results to then be submitted to the Pool.
         * There can be many WorkSolution objects generated from a single Work instance.
         * A solution object does not necessarily need to be submitted.
         * WorkSolution objects must be created through this function in order to be properly tied to a Work object (via the private protocolData member)
         * That way the pool protocol implementation can track which solution belongs to which work package.
         * A pool can make work objects expire (which can be queried via Work::expired()). This may be used as a hint to abort any calculations
         * and acquire a new work object, however it is not required to do so.
         * There is no obligation to check for expiration before submitting a solution
         *
         * @tparam WorkSolutionT Type of the Solution object that corresponds to this work's dynamic type (e.g. WorkSolutionEthash for WorkEthash).
         *
         * @return a WorkSolutionT wrapped in a unique_ptr in order to allow passing it later in a type erased fashion (as unique_ptr<WorkSolution>) without slicing
         */
        template<class WorkSolutionT>
        unique_ptr<WorkSolutionT> makeWorkSolution() const {
            return std::make_unique<WorkSolutionT>(*downCast<const typename WorkSolutionT::work_type>());
        }
    };
}
