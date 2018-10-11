
#pragma once

#include <deque>
#include <src/util/LockUtils.h>
#include <src/util/Logging.h>
#include <src/common/Pointers.h>
#include <src/common/Optional.h>
#include <src/common/Assert.h>

namespace miner {

    //thread safe container that can be used for queueing work and workresult unique_ptrs
    template<class T>
    class WorkQueue {

        std::mutex mutex;
        std::condition_variable conditionVariable;

        size_t limit = 0;
        std::deque<T> buffer;

        //returns lock if work is available or nullopt on timeout
        optional<std::unique_lock<std::mutex>> waitUntilNotEmptyAndLock(std::chrono::milliseconds timeout) {
            std::unique_lock<std::mutex> lock(mutex);

            bool timedOut = !conditionVariable.wait_for(lock, timeout, [this] {
                return !buffer.empty();
            });

            if (timedOut)
                return nullopt;
            return lock;
        }

    public:

        explicit WorkQueue(size_t limit = std::numeric_limits<size_t>::max())
        : limit(limit) {
        }

        size_t size() const {
            std::lock_guard<std::mutex> lock(mutex);
            return buffer.size();
        }

        bool empty() const {
            std::lock_guard<std::mutex> lock(mutex);
            return buffer.empty();
        }

        void clear() {
            std::lock_guard<std::mutex> lock(mutex);
            buffer.clear();
        }

        //push_back of an item, returns number of elements afterwards
        size_t pushBack(T &&t) {
            size_t size = 0;
            {
                std::lock_guard<std::mutex> lock(mutex);

                if (buffer.size() >= limit)
                    buffer.pop_back();

                buffer.push_front(std::move(t));
                size = buffer.size();
            }
            conditionVariable.notify_one();
            return size;
        }

        optional<T> popBackTimeout() {
            auto timeout = std::chrono::milliseconds(30);

            if (auto maybeLock = waitUntilNotEmptyAndLock(timeout)) {
                auto t {std::move(buffer.back())};
                buffer.pop_back();
                return t;
            }
            return nullopt; //timeout
        }

        optional<T> popFrontTimeout() {
            auto timeout = std::chrono::milliseconds(30);

            if (auto maybeLock = waitUntilNotEmptyAndLock(timeout)) {
                auto t {std::move(buffer.front())};
                buffer.pop_front();
                return t;
            }
            return nullopt; //timeout
        }

    };

}
