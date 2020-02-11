//
//

#include "CLI.h"
#include <string>
#include <src/algorithm/Algorithm.h>
#include <src/pool/Pool.h>
#include <sstream>
#include <src/util/StringUtils.h>

namespace miner {

    namespace {constexpr auto endl = '\n';}

    std::string commandEmpty() {
        std::stringstream out;
        out << commandListDevices();
        out << commandListAlgoImpls();
        out << commandListPoolImpls();
        return out.str();
    }

    std::string commandListDevices() {
        std::stringstream out;
        out << "available devices: " << endl;

        ComputeModule compute(Config{});

        size_t i = 0;
        for (auto &id : compute.getAllDeviceIds()) {
            out << "\tindex: " << i << ", name: '" << id.getName() << "', vendor: '" << stringFromVendorEnum(id.getVendor()) << "'\n";
            ++i;
        }
        out << i << " total\n";
        return out.str();
    }

    std::string commandListAlgoImpls() {
        std::stringstream out;
        out << "available algoImpls: " << endl;
        size_t i = 0;
        for (auto &entry : Algorithm::factoryEntries()) {
            out << "\t" << entry.algoImplName << "\t(for PowType '" << entry.powType << "')" << endl;
            ++i;
        }
        out << i << " total\n";
        return out.str();
    }

    std::string commandListPoolImpls() {
        std::stringstream out;
        out << "available poolImpls: " << endl;
        size_t i = 0;
        for (auto &entry : Pool::factoryEntries()) {
            out << "\t" << entry.poolImplName << "\t(for PowType '" << entry.powType << "')" << endl;
            ++i;
        }
        out << i << " total\n";
        return out.str();
    }

    optional<size_t> argIndex(const std::string &argName, int argc, const char **argv) {
        for (int i = 1; i < argc; ++i) {
            if (std::string{argv[i]} == toLower(argName))
                return i;
        }
        return nullopt;
    }

    bool hasArg(const std::vector<std::string> &argNames, int argc, const char **argv) {
        for (auto &name : argNames) {
            if (argIndex(name, argc, argv))
                return true;
        }
        return false;
    }

    optional<std::string> getPathAfterArg(const std::string &minusminusArg, int argc, const char **argv) {
        //minusminusArg is something like "--config"
        if (auto i = argIndex(minusminusArg, argc, argv)) {
            size_t next = *i + 1;
            if (next < argc)
                return std::string{argv[next]};
        }
        return nullopt;
    }

}