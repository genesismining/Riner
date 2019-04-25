
#pragma once

#include <src/common/Optional.h>
#include <src/common/Pointers.h>
#include <src/common/Assert.h>
#include <src/util/Logging.h>
#include <queue>
#include <functional>
#include <chrono>
#include <future>

namespace miner {

    template<class T>
    class AutoRefillQueue {
    public:

        //the RefillFunc callback is called without holding a lock.
        //parameters:
        //  out: empty vector that the new elements should be pushed into
        //  master: mutable reference to master element, which is intended to be incremented and replicated in some way
        //  currentSize: last known size of the internal queue (use it to decide how many new elements to push)
        using RefillFunc = std::function<void(std::vector<T> &out, T &master, size_t currentSize)>;

        using value_type = T;

        //provide a RefillFunc that is async called and should create new elements based on the master element
        //the refill callback is called once a master element has been provided and the element count goes below
        //refillThreshold

        AutoRefillQueue(size_t refillThreshold, RefillFunc &&refillFunc)
        : refillThreshold(refillThreshold) {

            refillTask = std::async(std::launch::async, [this, refillFunc = std::move(refillFunc), refillThreshold] {

                //toBeFilled and borrowedMaster serve as refillFunc's args while not holding the lock
                std::vector<T> toBeFilled;
                toBeFilled.reserve(refillThreshold); //rough size guess
                unique_ptr<T> borrowedMaster;
                std::unique_lock<std::mutex> lock(mutex);

                while (true) {
                    size_t currentSize = 0;

                    notifyNeedsRefill.wait(lock, [&] {
                        bool masterExists = master != nullptr;
                        bool belowThreshold = buffer.size() < refillThreshold;

                        return (belowThreshold && masterExists) || shutdown;
                    });

                    if (shutdown)
                        return; //end this task

                    borrowedMaster = std::move(master); //move master out of the lock scope
                    currentSize = buffer.size();

                    lock.unlock();
                    MI_EXPECTS(borrowedMaster != nullptr);
                    refillFunc(toBeFilled, *borrowedMaster, currentSize);
                    lock.lock();

                    //return the master, but only if it wasn't replaced by a new one while we borrowed it
                    if (master == nullptr)
                        master = std::move(borrowedMaster);

                    for (auto &newElement : toBeFilled) {
                        buffer.push_front(std::move(newElement));
                    }
                    toBeFilled.clear();

                    notifyNotEmptyAnymore.notify_all(); //notify popTimeout threads they will be able to
                    // acquire the lock once its unlocked in this task's notifyNeedsRefill.wait(...)
                }
            });
        }

        ~AutoRefillQueue() {
            {
                std::lock_guard<std::mutex> lock(mutex);
                shutdown = true;
            }
            notifyNeedsRefill.notify_one(); //wake up refillTask thread so it notices shutdown == true
        }

        //set "gold master" value that will be incremented and replicated by the refillFunc callback once the queue is
        //too empty. optional clearQueue parameter defines whether all existing replica of the previous master should be cleared
        //while holding the lock
        void setMaster(T &&newMaster, bool shouldClearQueue = false) {
            size_t currentSize = 0;
            {
                std::lock_guard<std::mutex> lock(mutex);
                master = std::make_unique<T>(std::move(newMaster));

                if (shouldClearQueue)
                    buffer.clear();

                currentSize = buffer.size();
            }

            if (currentSize < refillThreshold) {
                //notify is necessary even when buffer was not cleared, because of masterExists
                //condition in notifyNeedsRefill.wait(...)
                notifyNeedsRefill.notify_one();
            }
        }

        optional<T> popWithTimeout(std::chrono::steady_clock::duration timeoutDur) {
            size_t currentSize = 0;
            bool hasMaster = false;
            optional<T> result;

            {
                std::unique_lock<std::mutex> lock(mutex);

                bool timedOut = !notifyNotEmptyAnymore.wait_for(lock, timeoutDur, [this] {
                    return !buffer.empty();
                });

                if (timedOut)
                    return nullopt;

                result = std::move(buffer.back());
                buffer.pop_back();

                hasMaster = master != nullptr;
                currentSize = buffer.size();
            }//unlock

            if (currentSize < refillThreshold && hasMaster) {
                notifyNeedsRefill.notify_one();
            }

            return result;
        }


    private:
        std::mutex mutex;
        std::condition_variable notifyNotEmptyAnymore;
        std::condition_variable notifyNeedsRefill;

        std::deque<T> buffer;

        unique_ptr<T> master;

        size_t refillThreshold = 1; //once buffer.size() goes below this value, needs refill is notified
        bool shutdown = false;

        std::future<void> refillTask;
    };

}