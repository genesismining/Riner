//
//

#pragma once

#include <string>
#include <src/common/Optional.h>
#include <vector>

namespace miner {
    class ComputeModule;

    std::string commandList(bool asJson);
    std::string commandListDevices(bool asJson);
    std::string commandListAlgoImpls(bool asJson);
    std::string commandListPoolImpls(bool asJson);

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

}