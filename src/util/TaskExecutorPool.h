#pragma once

#include <condition_variable>
#include <future>
#include <queue>
#include <mutex>
#include <list>


namespace riner {

    /**
     * Launches some worker threads and provides functionality for pushing tasks to the pool,
     * which are then executed by the worker threads.
     */
	class TaskExecutorPool {

	protected:

		std::queue<std::function<void()>> jobQueue;
		std::mutex queueMutex;
		std::condition_variable cv;
		std::list<std::future<void>> workers;
		bool shutdown = false;

		void spawnTaskExecutor();

	public:
        /**
         * change the worker thread count after the object was already initialized
         * (setting the number of threads can instead also be done right in the constructor
         */
		void setThreadCount(size_t numThreads);

        /**
         * addTask that returns a value.
         * param fct task that will be executed by the worker threads as soon as possible
         * return a std::future<X> where X is fct's return type. Can be used to query if the task is done.
         **/
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

        /**
         * specialization of addTask for functions that return void.
         * param fct task that will be executed by the worker threads as soon as possible
         * return a std::future<void> for querying if the task is done.
         **/
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

        /**
         * Initialize the pool with the amount of hardware threads your machine has minus 1 workers.
         * If theres only one hardware thread, this constructor will still create 1 thread
         */
		inline TaskExecutorPool() : TaskExecutorPool(std::thread::hardware_concurrency() - 1) {
		}

        /**
         * Initialize the pool with a specific number of worker threads.
         * Will at minimum initialize one worker, even if 0 is passed.
         */
		explicit TaskExecutorPool(size_t numThreads);

        /**
         * joins all threads. Does not wait until all tasks were finished (may leave tasks unfinished)
         */
		~TaskExecutorPool();
	};

}
