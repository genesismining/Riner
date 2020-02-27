//
//

#pragma once

#include <iostream>
#include <future>
#include <condition_variable>

namespace riner {

    struct ShutdownState {

        bool shutdownRequested = false;
        bool applicationClosed = false; //whether the "class Application" instance is destroyed
        std::mutex mut;
        std::condition_variable notifyOnAbort;
        std::condition_variable notifyOnAppHasClosed;

        std::future<void> shutdownWithTimeoutTask;
    public:
        //runs on separate thread that communicates with the main() function and exits if the application does not close before a certain timeout
        void shutdownWithTimeoutFunc(int signum);

        void launchShutdownTask(int signum);

        void waitUntilShutdownRequested();

        void confirmAppHasClosed();

    };

}