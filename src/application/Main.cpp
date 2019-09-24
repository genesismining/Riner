
#include <iostream>
#include "Application.h"
#include <src/common/Optional.h>
#include <src/common/Pointers.h>
#include <future>
#include <condition_variable>
#include <src/util/Logging.h>

namespace miner {

    //this function is used to get a config path until we have a proper argc argv parser
    opt::optional<std::string> getPathAfterArg(cstring_span minusminusArg, int argc, const char *argv[]) {
        //minusminusArg is something like "--config"
        size_t pathI = 0;
        for (size_t i = 1; i < argc; ++i) {
            if (gsl::ensure_z(argv[i]) == minusminusArg) {
                pathI = i + 1; //path starts at next arg
                break;
            }
        }
        if (pathI == 0)
            return opt::nullopt;

        std::string path = argv[pathI];

        //if path contains escaped spaces
        while (!path.empty() && path.back() == '\\') {
            path.back() = ' '; //add space
            ++pathI;
            if (pathI < argc)
                path += argv[pathI];
        }

        return path;
    }


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

    START_EASYLOGGINGPP(argc, argv);

    shutdownState = std::make_unique<ShutdownState>();
    signal(SIGINT, sigintHandler); //uses shutdownState

    auto configPath = getPathAfterArg("--config", argc, argv);

    {
        Application app(configPath);
        shutdownState->waitUntilShutdownRequested();
    }//app destructor

    shutdownState->confirmAppHasClosed();
    shutdownState.reset(); //joins 'shutdownWithTimeoutTask' thread
    return 0;
}
