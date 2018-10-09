
#pragma once

#include <future>

namespace miner {

    //async call a T member function after a given future's task has returned.
    template<class Fn, class T, class ... Args>
    void asyncAppendTask(std::future<void> &task, std::launch policy, Fn &&fn, T *thisPtr, Args &&... args) {

        task = std::async(policy, [fn = std::forward<Fn &&>(fn), thisPtr, prevTask = std::move(task)] (Args &&... args) mutable {
            if (prevTask.valid())
                prevTask.wait();
            return (thisPtr->*std::forward<Fn>(fn))(std::forward<Args>(args) ...);
        }, std::forward<Args>(args) ...);
    }
}