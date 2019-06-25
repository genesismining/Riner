//
//
#pragma once

#include <future>
#include <mutex>

namespace miner {

    //a barrier object that multiple threads can wait() on until another thread calls unblock()
    //probably crazy inefficient. Used for writing tests
    class Barrier {
        std::promise<void> promise;
        std::shared_future<void> sharedFuture;
        mutable std::mutex copyFutureMutex;

        std::shared_future<void> copyFuture() const;
    public:
        Barrier();

        //wait from one or more threads (no spurious wakes)
        //see std::shared_future documentation for specifics
        void wait() const {
            copyFuture().wait();
        }

        //wait from one or more threads with timeout (no spurious wakes)
        //return value can be used to find out if timeout happened
        //see std::shared_future documentation for specifics
        std::future_status wait_for(std::chrono::milliseconds duration) const {
            return copyFuture().wait_for(duration);
        }

        //lets the threads waiting at wait() continue. after calling this, wait() returns immediately
        //see std::promise documentation for specifics
        void unblock() {
            promise.set_value();
        }

    };

}