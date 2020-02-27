#pragma once

#include <condition_variable>
#include <future>
#include <queue>
#include <mutex>
#include <list>


namespace riner {

	class TaskExecutorPool {

	protected:

		std::queue<std::function<void()>> jobQueue;
		std::mutex queueMutex;
		std::condition_variable cv;
		std::list<std::future<void>> workers;
		bool shutdown = false;

		void spawnTaskExecutor();

	public:

		void setThreadCount(size_t numThreads);

		template<typename F, std::enable_if_t<!std::is_same<std::result_of_t<F &&()>, void>::value, int> N = 0>
		std::future<std::result_of_t<F &&()>> addTask(F &&fct) {
            auto promise = std::make_shared<std::promise<std::result_of_t<F &&()>>>();
            auto future = promise->get_future();
            {
                std::lock_guard<std::mutex> lock(queueMutex);
                jobQueue.emplace([promise = std::move(promise), fct = std::forward<F>(fct)] {
                    promise->set_value(fct());
                });
            }
            cv.notify_one();
            return future;
        }

		template<typename F, std::enable_if_t<std::is_same<std::result_of_t<F &&()>, void>::value, int> N = 0>
		std::future<std::result_of_t<F &&()>> addTask(F &&fct) {
            auto promise = std::make_shared<std::promise<std::result_of_t<F &&()>>>();
            auto future = promise->get_future();
            {
                std::lock_guard<std::mutex> lock(queueMutex);
                jobQueue.emplace([promise = std::move(promise), fct = std::forward<F>(fct)] {
                    fct();
                    promise->set_value();
                });
            }
            cv.notify_one();
            return future;
        }

		inline TaskExecutorPool() : TaskExecutorPool(std::thread::hardware_concurrency() - 1) {
		}

		explicit TaskExecutorPool(size_t numThreads);

		~TaskExecutorPool();
	};

}