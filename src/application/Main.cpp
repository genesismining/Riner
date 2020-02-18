
#include <iostream>
#include "Application.h"
#include <src/common/Optional.h>
#include <src/common/Pointers.h>
#include <src/util/Logging.h>
#include <src/application/CLI.h>
#include <src/application/ShutdownState.h>

namespace miner {
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

    bool did_respond_already = false;

    //CLI
    bool format_as_json = hasArg({"--json"}, argc, argv);

    if (hasArg({"--help", "-h"}, argc, argv)) {
        std::cout << commandList(format_as_json);
        did_respond_already = true;
    }

    if (hasArg({"--list", "-l"}, argc, argv)) {
        std::cout << commandList(format_as_json);
        did_respond_already = true;
    }

    if (hasArg({"--list-devices"}, argc, argv)) {
        std::cout << commandListDevices(format_as_json);
        did_respond_already = true;
    }

    if (hasArg({"--list-algoimpls"}, argc, argv)) {
        std::cout << commandListAlgoImpls(format_as_json);
        did_respond_already = true;
    }

    if (hasArg({"--list-poolimpls"}, argc, argv)) {
        std::cout << commandListPoolImpls(format_as_json);
        did_respond_already = true;
    }

    if (hasArg({"--config"}, argc, argv)) {
        if (optional<std::string> configPath = getPathAfterArg("--config", argc, argv)) {

            if (optional<Config> config = configUtils::loadConfig(*configPath)) {

                Application app{std::move(*config)};
                shutdownState->waitUntilShutdownRequested();

            } //app destructor
            else {
                LOG(ERROR) << "no valid config available";
            }
        } else {
            LOG(ERROR) << "missing path after --config argument (--config /path/to/config.textproto)";
        }
    }
    else {
        if (!did_respond_already)
            LOG(ERROR) << "no config path command line argument (--config /path/to/config.textproto)";
    }

    shutdownState->confirmAppHasClosed();
    shutdownState.reset(); //joins 'shutdownWithTimeoutTask' thread
    return 0;
}
