
#include <iostream>
#include "Application.h"
#include <src/common/Optional.h>
#include <src/common/Pointers.h>
#include <src/util/Logging.h>
#include <src/application/CLI.h>
#include <src/application/ShutdownState.h>

namespace riner {
    unique_ptr<ShutdownState> shutdownState; //global, so that the SIGINT handler can access it
}

void sigintHandler(int signum) {
    riner::shutdownState->launchShutdownTask(signum);
}

int main(int raw_argc, const char *raw_argv[]) {
    using namespace riner;

    //expand args like -abc to -a -b -c
    auto expandedArgs = copyArgsAndExpandSingleDashCombinedArgs(raw_argc, raw_argv);
    int          argc = expandedArgs.argc;
    const char **argv = expandedArgs.argv;

    bool format_as_json = hasArg({"--json"}, argc, argv);
    bool use_log_colors = hasArg({"--color", "--colors"}, argc, argv);
    bool use_log_emojis = hasArg({"--emoji", "--emojis"}, argc, argv);
    initLogging(argc, argv, use_log_colors, use_log_emojis);
    setThreadName("main");

    VLOG(5) << "interpreting args as: " << expandedArgs.allArgsAsOneString;

    //register signal handlers that respond to ctrl+c abort
    //abort code can be found in ShutdownState
    shutdownState = std::make_unique<ShutdownState>();
    signal(SIGINT, sigintHandler); //uses shutdownState
    signal(SIGTERM, sigintHandler);

    bool did_respond_already = false; //if we responded to something we won't complain about e.g. "--config" missing

    if (hasArg({"--help", "-h"}, argc, argv)) {
        LOG(ERROR) << "help command not implemented yet";
        //std::cout << commandList(format_as_json);
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

                try {
                    Application app{std::move(*config)};
                    shutdownState->waitUntilShutdownRequested();
                }
                catch(const std::exception &e) {
                    LOG(ERROR) << "uncaught exception: " << e.what();
                }

            } //app destructor
            else {
                LOG(ERROR) << "no valid config available";
            }
        } else {
            LOG(ERROR) << "missing path after --config argument (--config /path/to/config.textproto)";
        }
    }
    else {
        if (!did_respond_already) //no other valid command line arg was used that we responded to, so tell the user what to do.
            LOG(ERROR) << "no config path command line argument (--config /path/to/config.textproto)";
    }

    shutdownState->confirmAppHasClosed();
    shutdownState.reset(); //joins 'shutdownWithTimeoutTask' thread
    VLOG(4) << "returning from main function";
    return 0;
}
