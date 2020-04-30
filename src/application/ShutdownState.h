//
//

#pragma once

#include <iostream>
#include <future>
#include <condition_variable>

namespace riner {

    /**
     * Responds to CTRL+C signal and shuts down the Application while giving it a little grace period to join all the threads and close all connections.
     * created in Main.cpp
     */
    struct ShutdownState {

        bool shutdownRequested = false;
        bool applicationClosed = false; //whether the "class Application" instance is destroyed
        std::mutex mut;
        std::condition_variable notifyOnAbort;
        std::condition_variable notifyOnAppHasClosed;

        std::future<void> shutdownWithTimeoutTask;
    public:
        /**
         * runs on separate thread that communicates with the main() function and exits if the application does not close before a certain timeout
         */
        void shutdownWithTimeoutFunc(int signum);

        /**
         * Launches the shutdown thread which gives the application a little grace period to shutdown and
         * also unblocks the `waitUntilShutdownRequested()` function which the main thread in `main()` is waiting in.
         * called by the sigintHandler in Main.cpp.
         * param signum the signum argument of the signal handler that calls this function
         */
        void launchShutdownTask(int signum);

        /**
         * blocks until launchShutdownTask() is called
         */
        void waitUntilShutdownRequested();

        /**
         * called by the main thread to communicate to the Shutdown task thread that the `Applicatoin` successfully closed,
         * which causes the Shutdown task to finish and join.
         */
        void confirmAppHasClosed();

    };

}
