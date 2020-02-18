//
//

#include "ShutdownState.h"

namespace miner {

    void ShutdownState::shutdownWithTimeoutFunc(int signum) {
        std::unique_lock <std::mutex> lock(mut);
        shutdownRequested = true;
        notifyOnAbort.notify_one();

        auto timeout = std::chrono::seconds(10);
        std::cout << "waiting for application to close" << std::endl;

        //unlock mut
        notifyOnAppHasClosed.wait_for(lock, timeout, [this] {
            return applicationClosed;
        });

        if (!applicationClosed) {
            std::cout << "application did not close before timeout, exiting" << std::endl;
            _Exit(signum);
        } else {
            std::cout << "application has closed in time" << std::endl;
        }
    }

    void ShutdownState::launchShutdownTask(int signum) {
        shutdownWithTimeoutTask = std::async(std::launch::async, [this, signum] {
            shutdownWithTimeoutFunc(signum);
        });
    }

    void ShutdownState::waitUntilShutdownRequested() {
        std::unique_lock <std::mutex> lock(mut);
        notifyOnAbort.wait(lock, [this] {
            return shutdownRequested;
        });
    }

    void ShutdownState::confirmAppHasClosed() {
        {
            std::lock_guard <std::mutex> lock(mut);
            applicationClosed = true;
        }
        notifyOnAppHasClosed.notify_one();
    }
};