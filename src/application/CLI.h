//
//

#pragma once

#include <string>
#include <src/common/Optional.h>
#include <vector>

namespace riner {
    class ComputeModule;

    /**
     * convenience typedef for that one usage in Main.cpp.
     */
    using CommandInfos = std::vector<std::pair<std::vector<std::string>, std::string>>;

    /**
     * command line interface functions that return a std::string which can then be output on std out and/or logged
     * see table in Main.cpp's main() function to see a short description of what each of them does (or run riner with --help)
     */
    std::string commandHelp(const CommandInfos &, bool asJson);
    std::string commandList(bool asJson);
    std::string commandListDevices(bool asJson);
    std::string commandListAlgoImpls(bool asJson);
    std::string commandListPoolImpls(bool asJson);

    /**
     * command line argument wrapper struct. Used as return value for for `copyArgsAndExpandSingleDashCombinedArgs`
     */
    struct CommandLineArgs {
        std::string allArgsAsOneString; //"-a -b --foo"
        std::vector<std::string> strings; //{"-a", "-b", "--foo"}
        std::vector<const char *> ptrs; //{data[0].c_str(), data[1].c_str(), ... }

        const char **argv = nullptr; //&ptrs[0]
        int argc = 0; //data.size()
    };

    /**
     * function that splits up single hyphen command line argument letters into individual single hyphen args.
     * e.g. takes {"-a" "-bcd" "-e" "-fg" "--hij"} and returns {"-a", "-b", "-c", "-d", "-e" "-f" "-g" "--hij"}
     */
    CommandLineArgs copyArgsAndExpandSingleDashCombinedArgs(int argc, const char **argv);

    /**
     * creates a "tutorial_config.textproto" file in the executable's directory and adds a "--config=" command to
     * the provided commandline args that runs that config
     * If the tutorial_config already exists, we do not overwrite it, since the person running the tutorial
     * is likely just tinkering with it to gain a better understanding.
     * param args the current command line args
     * return the command line args with added (or replaced) "--config=" command that now points to the generated tutorial_config.textproto
     */
    CommandLineArgs commandRunTutorial(const CommandLineArgs &args);

    /**
     * reports unsupported commands on command line and returns amount of them
     * param supportedCommands table of supported commands as seen in Main.cpp's `main()`
     * param argc argc as given by `main()`
     * param argv argv as given by `main()`
     */
    size_t reportUnsupportedCommands(const CommandInfos &supportedCommands, int argc, const char **argv);

    /**
     * searches for argName in argv.
     * @param argc amount of elements in argv array
     * @param argv ptr to first command line argument
     * @return index of argName in argv if argName was found, nullopt otherwise
     */
    optional<size_t> argIndex(const std::string &argName, int argc, const char **argv);

    /**
     * return whether one of the given `argNames` exists in argv (see usage in Main.cpp)
     */
    bool hasArg(const std::vector<std::string> &argNames, int argc, const char **argv);

    /**
     * obsolete function
     * return the argument after `minusminusArg` if `minusminusArg` exists among `argv`, nullopt otherwise
     */
    optional<std::string> getPathAfterArg(const std::string &minusminusArg, int argc, const char **argv);

    /**
     * if an arg has an equals sign (e.g. -v=3) get the part after the equals sign.
     * param argNames a list of argnames to match, e.g. {"-v", "--verbose", "--VERBOSE"}
     * param argc argc as given by `main()`
     * param argv argv as given by `main()`
     * return the part after the equals sign (e.g. "3") or nullopt if the arg has no equals sign (empty string if it has an equals sign but nothing behind it)
     */
    optional<std::string> getValueAfterArgWithEqualsSign(const std::vector<std::string> &argNames, int argc, const char **argv);

}
