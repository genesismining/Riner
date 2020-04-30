//
//

#include "ShutdownState.h"
#include <src/util/Logging.h>

namespace riner {

    void ShutdownState::shutdownWithTimeoutFunc(int signum) {
        std::unique_lock <std::mutex> lock(mut);
        shutdownRequested = true;
        notifyOnAbort.notify_one();

        using namespace std::chrono;
        auto timeout = seconds(10);
        auto before = steady_clock::now();
        std::cout << "\nwaiting for application to close" << std::endl;
        VLOG(3) << "shutdown timeout = " << timeout.count() << "s";

        //unlock mut
        notifyOnAppHasClosed.wait_for(lock, timeout, [this] {
            return applicationClosed;
        });

        auto after = steady_clock::now();

        float elapsed_sec = float(duration_cast<milliseconds>(after - before).count() / 100) * 0.1f;

        if (!applicationClosed) {
            VLOG(3) << elapsed_sec << " seconds passed";
            std::cout << "application did not close before timeout, exiting" << std::endl;
            _Exit(signum);
        } else {
            LOG(INFO) << "application has closed";
            VLOG(1) << "application closed within " << elapsed_sec << "s";
        }
    }

    void ShutdownState::launchShutdownTask(int signum) {
        shutdownWithTimeoutTask = std::async(std::launch::async, [this, signum] {
            SetThreadNameStream{} << "shutdown state";
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
        VLOG(6) << "shutdown state confirmAppHasClosed...";
        {
            std::lock_guard <std::mutex> lock(mut);
            applicationClosed = true;
        }
        notifyOnAppHasClosed.notify_one();
        VLOG(6) << "shutdown state confirmAppHasClosed...done";
    }
};