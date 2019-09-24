
#pragma once

#include "Pool.h"
#include "Work.h"
#include <src/common/Optional.h>
#include <src/common/Pointers.h>
#include <src/common/Assert.h>
#include <src/util/Logging.h>
#include <atomic>
#include <queue>
#include <list>
#include <functional>
#include <chrono>
#include <future>


namespace miner {

    class LazyWorkQueue {

        friend struct PoolJob;

    protected:
        std::atomic_int64_t latestJobId {0};
        std::mutex mutex;
        std::deque<std::shared_ptr<PoolJob>> jobQueue;

    public:

        void setMaster(std::unique_ptr<PoolJob> newMaster, bool cleanFlag = false) {

            std::lock_guard<std::mutex> lock(mutex);
            if (cleanFlag) {
                jobQueue.clear();
            }
            else {
                //limit the max. size of the jobQueue so that really old jobs are dropped
                jobQueue.resize(std::min(jobQueue.size(), size_t(7)));
            }
            newMaster->id = latestJobId.fetch_add(1, std::memory_order_relaxed) + 1;
            jobQueue.emplace_front(std::move(newMaster));

        }

        unique_ptr<Work> tryGetWork() {
            std::unique_lock<std::mutex> lock(mutex);
            if (jobQueue.empty())
                return nullptr;
            std::unique_ptr<Work> work = jobQueue.front()->makeWork();
            work->job = jobQueue.front();
            return work;
        }

        inline bool isExpiredJob(const PoolJob &job) {
            return latestJobId.load(std::memory_order_relaxed) != job.id;
        }
    };


    class WorkQueue {

        friend struct PoolJob;

    protected:
        std::atomic_int64_t latestJobId {0};
        std::mutex mutex;
        std::deque<std::shared_ptr<PoolJob>> jobQueue;

    public:

        WorkQueue(size_t refillThreshold = 8, size_t maxWorkQueueLength = 16)
                : refillThreshold(refillThreshold) {

            refillTask = std::async(std::launch::async, [this, refillThreshold, maxWorkQueueLength] {

                //toBeFilled and borrowedMaster serve as refillFunc's args while not holding the lock
                std::vector<std::unique_ptr<Work>> toBeFilled;
                toBeFilled.reserve(maxWorkQueueLength); //rough size guess
                std::unique_lock<std::mutex> lock(mutex);
                int64_t lastId = -1;

                while (true) {
                    size_t currentSize = 0;

                    notifyNeedsRefill.wait(lock, [&] {
                        bool masterExists = !jobQueue.empty();
                        bool hasNewMaster = masterExists && lastId != jobQueue.front()->id;
                        bool belowThreshold = buffer.size() < refillThreshold;

                        return (belowThreshold && masterExists) || hasNewMaster || shutdown;
                    });

                    if (shutdown)
                        return; //end this task

                    MI_EXPECTS(!jobQueue.empty());
                    auto master = jobQueue.front();
                    bool hasNewMaster = lastId != master->id;
                    lastId = master->id;
                    currentSize = buffer.size();

                    lock.unlock();
                    for (size_t size = hasNewMaster ? 0 : currentSize; size < maxWorkQueueLength; size++) {
                        toBeFilled.push_back(master->makeWork());
                        toBeFilled.back()->job = master;
                    }
                    lock.lock();

                    //if a new master was set then ignore the newly generated work
                    if (!jobQueue.empty() && lastId == jobQueue.front()->id) {
                        if (hasNewMaster) {
                            buffer.clear();
                        }

                        for (auto &newElement : toBeFilled) {
                            buffer.push_front(std::move(newElement));
                        }

                        if (hasNewMaster) {
                            latestJobId.store(lastId);
                        }

                        notifyNotEmptyAnymore.notify_all(); //notify popTimeout threads they will be able to
                        // acquire the lock once its unlocked in this task's notifyNeedsRefill.wait(...)
                    }
                    toBeFilled.clear();
                }
            });
        }

        ~WorkQueue() {
            {
                std::lock_guard<std::mutex> lock(mutex);
                shutdown = true;
            }
            notifyNeedsRefill.notify_one(); //wake up refillTask thread so it notices shutdown == true
        }

        //set "gold master" value that will be incremented and replicated by the refillFunc callback once the queue is
        //too empty. cleanFlag parameter defines whether all existing replica of the previous master should be cleared
        //while holding the lock
        void setMaster(std::unique_ptr<PoolJob> newMaster, bool cleanFlag = false) {
            {
                std::lock_guard<std::mutex> lock(mutex);
                if (cleanFlag) {
                    jobQueue.clear();
                    buffer.clear();
                }
                else {
                    //limit the max. size of the jobQueue so that really old jobs are dropped
                    jobQueue.resize(std::min(jobQueue.size(), size_t(7)));
                }
                newMaster->id = ++currentId;
                jobQueue.emplace_front(std::move(newMaster));
            }

            notifyNeedsRefill.notify_one();
        }

        unique_ptr<Work> popWithTimeout(std::chrono::steady_clock::duration timeoutDuration = std::chrono::milliseconds(100)) {
            size_t currentSize = 0;
            bool hasMaster = false;
            unique_ptr<Work> result;

            {
                std::unique_lock<std::mutex> lock(mutex);

                bool timedOut = !notifyNotEmptyAnymore.wait_for(lock, timeoutDuration, [this] {
                    return !buffer.empty();
                });

                if (timedOut)
                    return nullptr;

                result = std::move(buffer.back());
                buffer.pop_back();

                hasMaster = !jobQueue.empty();
                currentSize = buffer.size();
            }//unlock

            if (currentSize < refillThreshold && hasMaster) {
                notifyNeedsRefill.notify_one();
            }

            return result;
        }

        inline bool isExpiredJob(const PoolJob &job) {
            return latestJobId.load(std::memory_order_relaxed) != job.id;
        }

    protected:

        bool shutdown {false};

    private:
        std::condition_variable notifyNotEmptyAnymore;
        std::condition_variable notifyNeedsRefill;
        int64_t currentId {0};

        std::deque<std::unique_ptr<Work>> buffer;

        size_t refillThreshold; //once buffer.size() goes below this value, needs refill is notified

        std::future<void> refillTask;
    };

}
