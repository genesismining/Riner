//
//

#pragma once

#include <future>

namespace riner {

    using std::launch;

    /**
     * Append another async task to an existing future
     *
     * @param inout future that the task is supposed to be appended to
     * @param policy see std::async launch policy documentation
     * @param func a callable with a defined operator()(Args...) that is to be called after inout.wait() has returned
     */
    template<class Fn, class... Args>
    void asyncAppend(std::future<void> &inout, std::launch policy, Fn &&func, Args &&... args) {

        inout = std::async(policy, [fut = std::move(inout), func = std::forward<Fn>(func)] (Args &&... args) {
            if (fut.valid())
                fut.wait();
            func(std::forward<Args>(args) ...);
        }, std::forward<Args>(args) ...);
    }

}
