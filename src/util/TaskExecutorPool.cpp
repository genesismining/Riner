#include "TaskExecutorPool.h"

namespace miner {

    void TaskExecutorPool::spawnTaskExecutor() {
        workers.emplace_back(std::async(std::launch::async, [this]() {
            while (true) {
                std::function<void()> job;
                {
                    std::unique_lock<std::mutex> lock(queueMutex);

                    cv.wait(lock, [&] { return !jobQueue.empty() || shutdown; });
                    if (shutdown)
                        return;
                    job = std::move(jobQueue.front());
                    jobQueue.pop();
                }
                job(); // function<void()> type
            }
        }));
    }

    void TaskExecutorPool::setThreadCount(size_t numThreads) {
        if (numThreads > 0 && workers.size() > numThreads) {
            {
                std::lock_guard<std::mutex> lk(queueMutex);
                shutdown = true;
            }
            cv.notify_all();
            workers.clear();
            shutdown = false;
        }
        for (size_t i = workers.size(); i < numThreads; i++) {
            spawnTaskExecutor();
        }
    }

    TaskExecutorPool::TaskExecutorPool(size_t numThreads) {
        for (size_t i = 0; i < std::max(numThreads, size_t(1)); i++) {
            spawnTaskExecutor();
        }
    }

    TaskExecutorPool::~TaskExecutorPool() {
        {
            std::lock_guard<std::mutex> lock(queueMutex);
            shutdown = true;
        }
        cv.notify_all();
    }

}