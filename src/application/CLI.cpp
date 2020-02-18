//
//

#include "CLI.h"
#include <string>
#include <src/algorithm/Algorithm.h>
#include <src/pool/Pool.h>
#include <sstream>
#include <src/util/StringUtils.h>
#include <src/application/Registry.h>

#include <src/common/Json.h>

namespace miner {

    namespace {
        constexpr auto endl = '\n';
        constexpr auto json_indent = 4;
    }

    std::string commandList(bool useJson) {
        if (useJson) {
            return nl::json {
                {"devices", nl::json::parse(commandListDevices(useJson))},
                {"algoImpls", nl::json::parse(commandListAlgoImpls(useJson))},
                {"poolImpls", nl::json::parse(commandListPoolImpls(useJson))},
            }.dump(json_indent);
        }
        else {
            std::stringstream ss;
            ss << commandListDevices(useJson);
            ss << commandListAlgoImpls(useJson);
            ss << commandListPoolImpls(useJson);
            return ss.str();
        }
    }

    //returns empty string if n == 1, "s" otherwise. use for appending plural 's' to word(s)
    std::string maybePluralS(int n) {
        return n == 1 ? "" : "s";
    }

    std::string commandListDevices(bool useJson) {
        ComputeModule compute(Config{});
        auto &allIds = compute.getAllDeviceIds();
        size_t i = 0;

        if (useJson) {
            nl::json j;
            for (auto &id : allIds) {
                j.push_back(nl::json{
                        {"index",  i},
                        {"name",   id.getName()},
                        {"vendor", stringFromVendorEnum(id.getVendor())},
                });
                ++i;
            }
            return j.dump(json_indent);
        }
        else { //readable
            std::stringstream ss;
            ss << allIds.size() << " available device" << maybePluralS(allIds.size()) << ": " << endl;
            for (auto &id : allIds) {
                ss << "\tindex: " << i << ", name: '" << id.getName() << "', vendor: '"
                   << stringFromVendorEnum(id.getVendor()) << "'" << endl;
                ++i;
            }
            return ss.str();
        }
    }

    std::string commandListAlgoImpls(bool useJson) {
        auto all = Registry{}.listAlgoImpls();
        size_t i = 0;

        if (useJson) {
            nl::json j;
            for (auto &e : all) {
                j.push_back(nl::json{
                        {"name", e.name},
                        {"powType", e.powType},
                });
                ++i;
            }
            return j.dump(json_indent);
        }
        else {
            std::stringstream ss;
            ss << all.size() << " available algoImpl" << maybePluralS(all.size()) << ": " << endl;
            for (auto &listing : all) {
                ss << "\t'" << listing.name << "'\t\t(for PowType '" << listing.powType << "')" << endl;
                ++i;
            }
            return ss.str();
        }
    }

    std::string commandListPoolImpls(bool useJson) {
        auto all = Registry{}.listPoolImpls();
        size_t i = 0;

        if (useJson) {
            nl::json j;
            for (auto &e : all) {
                j.push_back(nl::json{
                    {"name", e.name},
                    {"powType", e.powType},
                    {"protocolType", e.protocolType},
                    {"protocolTypeAlias", e.protocolTypeAlias},
                });
            }
            return j.dump(json_indent);
        }
        else {
            std::stringstream ss;
            ss << all.size() << " available poolImpl" << maybePluralS(all.size()) << ": " << endl;
            for (auto &e : all) {
                ss << "\t'" << e.name << "'\t\t(for PowType '" << e.powType << "'\t protocol names: ";
                ss << "'" << e.protocolType << "'";
                if (0 != strcmp("", e.protocolTypeAlias)) { //if it has an alias
                    ss << ", '" << e.protocolTypeAlias << "'";
                }
                ss << ")" << endl;
                ++i;
            }
            return ss.str();
        }
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