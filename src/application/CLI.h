//
//

#pragma once

#include <string>
#include <src/common/Optional.h>
#include <vector>

namespace riner {
    class ComputeModule;
    using CommandInfos = std::vector<std::pair<std::vector<std::string>, std::string>>;

    std::string commandHelp(const CommandInfos &, bool asJson);
    std::string commandList(bool asJson);
    std::string commandListDevices(bool asJson);
    std::string commandListAlgoImpls(bool asJson);
    std::string commandListPoolImpls(bool asJson);

    struct CommandLineArgs {
        std::string allArgsAsOneString; //"-a -b --foo"
        std::vector<std::string> strings; //{"-a", "-b", "--foo"}
        std::vector<const char *> ptrs; //{data[0].c_str(), data[1].c_str(), ... }

        const char **argv = nullptr; //&ptrs[0]
        int argc = 0; //data.size()
    };

    //takes {"-a" "-bcd" "-e" "-fg" "--hij"} and returns {"-a", "-b", "-c", "-d", "-e" "-f" "-g" "--hij"}
    CommandLineArgs copyArgsAndExpandSingleDashCombinedArgs(int argc, const char **argv);

    //reports unsupported commands on command line and returns amount of them
    size_t reportUnsupportedCommands(const CommandInfos &supportedCommands, int argc, const char **argv);

    /**
     * searches for argName in argv.
     * @param argc amount of elements in argv array
     * @param argv first command line argument
     * @return index of argName in argv if argName was found, nullopt otherwise
     */
    optional<size_t> argIndex(const std::string &argName, int argc, const char **argv);

    //returns whether one of the given args exists in argv
    bool hasArg(const std::vector<std::string> &argNames, int argc, const char **argv);

    //this function is used to get a config path until we have a proper argc argv parser
    optional<std::string> getPathAfterArg(const std::string &minusminusArg, int argc, const char **argv);

    //if an arg has an equals sign (e.g. -v=3) returns the string of the part after the equals sign (e.g. "3") or nullopt if the arg has no equals sign (empty string if it has an equals sign but nothing behind it)
    optional<std::string> getValueAfterArgWithEqualsSign(const std::vector<std::string> &argNames, int argc, const char **argv);

}