
#include <iostream>
#include "Application.h"
#include <src/common/Optional.h>
#include <src/common/Pointers.h>
#include <src/util/Logging.h>
#include <src/application/CLI.h>
#include <src/application/ShutdownState.h>

namespace riner {
    /**
     * ShutdownState deals with shutting down the application after SIGINT or SIGTERM. See more in ShutdownState.
     */
    unique_ptr<ShutdownState> shutdownState; //global, so that the SIGINT handler can access it
}

//registered in main()
void sigintHandler(int signum) {
    riner::shutdownState->launchShutdownTask(signum);
}

int main(int raw_argc, const char *raw_argv[]) {
    using namespace riner;
    using namespace std;

    //expand args like -abc to -a -b -c
    auto expandedArgs = copyArgsAndExpandSingleDashCombinedArgs(raw_argc, raw_argv);
    int          argc = expandedArgs.argc;
    const char **argv = expandedArgs.argv;

    vector<pair<vector<string>, string>> command_infos {
      //{{commands,...}, "help string"},
        {{"--help", "-h"}, "show help"},
        {{"--config"}, "--config=path/to/config.textproto run the mining algorithms as specified in the provided config file"},
        {{"--list", "-l"}, "list all devices, algoimpls and poolimpls"},
        {{"--list-devices"}, "list all available devices (gpus, cpus)"},
        {{"--list-algoimpls"}, "list all available algorithm implementations"},
        {{"--list-poolimpls"}, "list all available pool protocol implementations"},
        {{"--json"}, "modifies output to be formatted as json"},
        {{"--color", "--colors"}, "enables colored output on some terminals"},
        {{"--emoji", "--emojis"}, "enables unicode emoji symbols for log message severity"},
        {{"--verbose", "-v"}, "-v=X with X from 0-9. Set verbose logging level. -v enables max verbosity"},
        {{"--sec"}, "make log timestamp only consist of second and subsecond part"},
        {{"--logsize"}, "--logsize=X set size of one log file in bytes. writing of logs rotates between two files as one reaches the size limit."},
    };

    if (reportUnsupportedCommands(command_infos, argc, argv)) {
        exit(1);
    }

    bool format_as_json = hasArg({"--json"}, argc, argv);
    bool use_log_colors = hasArg({"--color", "--colors"}, argc, argv);
    bool use_log_emojis = hasArg({"--emoji", "--emojis"}, argc, argv);
    initLogging(argc, argv, use_log_colors, use_log_emojis);
    setThreadName("main"); //sets the thread name for logging

    VLOG(5) << "interpreting args as: " << expandedArgs.allArgsAsOneString;

    //ShutdownState: handles signals that respond to ctrl+c
    shutdownState = std::make_unique<ShutdownState>();
    signal(SIGINT, sigintHandler); //uses shutdownState
    signal(SIGTERM, sigintHandler);

    bool did_respond_already = false; //if we responded to something we won't complain about e.g. "--config" missing

    //check command line arguments and respond accordingly
    
    if (hasArg({"--help", "-h"}, argc, argv)) {
        std::cout << commandHelp(command_infos, format_as_json);
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

        if (hasArg({"--json"}, argc, argv))
            LOG(INFO) << "note: json formatting only applies to list commands.";

        if (optional<std::string> configPath = getValueAfterArgWithEqualsSign({"--config"}, argc, argv)) {

            if (optional<Config> config = configUtils::loadConfig(*configPath)) {

                try {
                    //app will launch here and run until the destructor.
                    Application app{std::move(*config)};
                    shutdownState->waitUntilShutdownRequested(); //this call will block until there is a SIGINT or SIGTERM interrupt (e.g. via CTRL+C
                }
                catch(const std::exception &e) {
                    LOG(ERROR) << "uncaught exception: " << e.what();
                }

            } //app destructor
            else {
                LOG(ERROR) << "no valid config available";
            }
        } else {
            LOG(ERROR) << "missing path after --config argument (--config=/path/to/config.textproto)";
        }
    }
    else {
        if (!did_respond_already) //no other valid command line arg was used that we responded to, so tell the user what to do.
            LOG(ERROR) << "no config path command line argument (--config=/path/to/config.textproto)";
    }

    shutdownState->confirmAppHasClosed();
    shutdownState.reset(); //joins 'shutdownWithTimeoutTask' thread
    VLOG(4) << "returning from main function";
    return 0;
}
