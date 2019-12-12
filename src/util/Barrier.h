//
//
#pragma once

#include <future>
#include <mutex>

namespace miner {

    //a barrier object that multiple threads can wait() on until another thread calls unblock()
    //probably crazy inefficient. Used for writing tests
    class Barrier {

        mutable std::mutex promise_set_value_mutex; //locked when promise.set_value is (maybe) called, and promise_has_value is set to true
        bool promise_has_value = false;
        std::promise<void> promise;

        std::shared_future<void> sharedFuture;
        mutable std::mutex copyFutureMutex; //locked when the copy ctor of sharedFuture is called

        std::shared_future<void> copyFuture() const;
    public:
        Barrier();
        ~Barrier() {
            unblock();
        }

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
            std::lock_guard<std::mutex> lock{promise_set_value_mutex};

            if (!promise_has_value) {
                promise.set_value();
                promise_has_value = true;
            }
        }

    };

}