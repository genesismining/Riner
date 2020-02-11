
#include <iostream>
#include "Application.h"
#include <src/common/Optional.h>
#include <src/common/Pointers.h>
#include <future>
#include <condition_variable>
#include <src/util/Logging.h>
#include <src/application/CLI.h>

namespace miner {

    struct ShutdownState {

        bool shutdownRequested = false;
        bool applicationClosed = false;
        std::mutex mut;
        std::condition_variable notifyOnAbort;
        std::condition_variable notifyOnAppHasClosed;

        std::future<void> shutdownWithTimeoutTask;
    public:
        //runs on separate thread that communicates with the main() function and exits if the application does not close before a certain timeout
        void shutdownWithTimeoutFunc(int signum) {
            std::unique_lock<std::mutex> lock(mut);
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

        void launchShutdownTask(int signum) {
            shutdownWithTimeoutTask = std::async(std::launch::async, [this, signum] {
                shutdownWithTimeoutFunc(signum);
            });
        }

        void waitUntilShutdownRequested() {
            std::unique_lock<std::mutex> lock(mut);
            notifyOnAbort.wait(lock, [this] {
                return shutdownRequested;
            });
        }

        void confirmAppHasClosed() {
            {
                std::lock_guard<std::mutex> lock(mut);
                applicationClosed = true;
            }
            notifyOnAppHasClosed.notify_one();
        }

    };

    unique_ptr<ShutdownState> shutdownState; //global, so that the SIGINT handler can access it
}

void sigintHandler(int signum) {
    miner::shutdownState->launchShutdownTask(signum);
}

int main(int argc, const char *argv[]) {
    using namespace miner;

    initLogging(argc, argv);
    setThreadName("main");

    shutdownState = std::make_unique<ShutdownState>();
    signal(SIGINT, sigintHandler); //uses shutdownState
    signal(SIGTERM, sigintHandler);

    //CLI
    if (hasArg({"--help", "-h"}, argc, argv)) {
        std::cout << commandEmpty();
    }

    if (hasArg({"--list-devices", "-l"}, argc, argv)) {
        std::cout << commandListDevices();
    }

    if (hasArg({"--list-algoimpls", "-l"}, argc, argv)) {
        std::cout << commandListAlgoImpls();
    }

    if (hasArg({"--list-poolimpls", "-l"}, argc, argv)) {
        std::cout << commandListPoolImpls();
    }

    if (optional<std::string> configPath = getPathAfterArg("--config", argc, argv)) {

        if (optional<Config> config = configUtils::loadConfig(*configPath)) {

            Application app{std::move(*config)};
            shutdownState->waitUntilShutdownRequested();

        } //app destructor
        else {
            LOG(ERROR) << "no valid config available";
        }
    } else {
        LOG(ERROR) << "no config path command line argument (--config /path/to/config.json)";
    }

    shutdownState->confirmAppHasClosed();
    shutdownState.reset(); //joins 'shutdownWithTimeoutTask' thread
    return 0;
}
