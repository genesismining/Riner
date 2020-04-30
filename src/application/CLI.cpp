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

namespace riner {

    namespace {
        constexpr auto endl = '\n';
        constexpr auto json_indent = 4;
    }

    std::string commandHelp(const CommandInfos &infos, bool useJson) {
        if (useJson) {
            nl::json j;
            for (auto &pair : infos) {
                j.emplace_back();
                auto &jinfo = j.back();

                auto &jnames = jinfo["commands"];
                for (auto &name : pair.first) {
                    jnames.push_back(name);
                }
                jinfo["help_text"] = pair.second;
            }
            return j.dump(json_indent);
        }
        else {
            std::vector<std::string> concat_names; //{"--help, -h", "--config", ...}
            size_t longest_concat = 0;

            for (auto &pair : infos) {
                concat_names.emplace_back();
                auto &concat_name = concat_names.back();

                for (auto &name : pair.first)
                    concat_name += ", " + name;

                if (concat_name.length() > 2)
                    concat_name = concat_name.c_str() + 2; //skip first comma

                if (longest_concat < concat_name.length())
                    longest_concat = concat_name.length();
            }

            std::stringstream ss;

            for (size_t i = 0; i < infos.size(); ++i) {
                auto &info = infos.at(i);
                auto &help_text = info.second;
                std::string concat_name = concat_names.at(i);
                //add space for alignment
                while (concat_name.length() < longest_concat) {
                    concat_name = " " + concat_name;
                }
                ss << concat_name << "  " << help_text << endl;
            }
            return ss.str();
        }
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
        VLOG(5) << "creating compute...";
        ComputeModule compute(Config{});
        VLOG(5) << "creating compute...done";
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
                if (e.protocolTypeAlias != "") { //if it has an alias
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
            std::string arg = partBefore("=", argv[i]);
            if (toLower(arg) == toLower(argName))
                return i;
        }
        return nullopt;
    }

    optional<std::string> argValueAfterEqualsSign(const std::string &argName, int argc, const char **argv) {
        for (int i = 1; i < argc; ++i) {
            std::string raw_arg = argv[i];
            std::string arg = raw_arg;
            auto firstEqPos = arg.find_first_of('=');

            optional<std::string> value;
            if (firstEqPos != std::string::npos) {
                arg   = raw_arg.substr(0, firstEqPos - 0);
                value = raw_arg.substr(firstEqPos + 1);
            }
            if (toLower(arg) == toLower(argName)) {
                return value;
            }
        }
        return nullopt;
    }

    CommandLineArgs copyArgsAndExpandSingleDashCombinedArgs(int argc, const char **argv) {
        CommandLineArgs res;
        res.argc = 0;
        if (argc <= 0)
            return res;

        for (int i = 0; i < argc; ++i) {
            std::string arg = argv[i];
            bool singleMinus = arg.size() >= 2 && arg[0] == '-' && arg[1] != '-';

            //whether its something like -v=0
            bool equalsSignAt2 = arg.size() >= 3 && arg[2] == '=';

            if (singleMinus && !equalsSignAt2) {
                //it is something like -abcd
                for (char c : arg) {
                    if (c != '=' && c != '-')
                        res.strings.push_back(std::string("-") + c);
                }
            } else { //its something like --something or -a=something
                res.strings.push_back(arg);
            }
        }
        //fill other members of result struct
        res.argc = (int) res.strings.size();
        for (auto &str : res.strings) {
            bool isFirstPathArg = &str == &res.strings.front();
            bool isLastArg = &str == &res.strings.back();

            if (!isFirstPathArg) {
                res.allArgsAsOneString += str;
                if (!isLastArg)
                    res.allArgsAsOneString += " ";
            }

            res.ptrs.push_back(str.c_str());
            res.argv = res.ptrs.data();
        }
        return res;
    }

    size_t reportUnsupportedCommands(const CommandInfos &infos, int argc, const char **argv) {
        std::vector<std::string> supported; //names of supported args
        for (auto &pair : infos)
            for (auto &cmd : pair.first)
                supported.push_back(cmd);

        std::vector<std::string> unsupported; //e.g. {arg0, arg4, arg8}
        std::string concat_unsupported; //e.g. "arg0, arg4, arg8"

        //skip first arg
        for (int i = 1; i < argc; ++i) {
            std::string arg = argv[i];
            arg = partBefore("=", arg); //take only the part of the arg before a possible '='

            bool is_supported = false;
            for (auto &supp : supported) {
                is_supported |= arg == supp;
            }

            if (!is_supported) {
                unsupported.push_back(arg);
                concat_unsupported += ", " + arg;
            }
        }

        //remove leading comma
        if (concat_unsupported.size() > 2)
            concat_unsupported = concat_unsupported.c_str() + 2;

        if (!unsupported.empty()) {
            for (auto &arg : unsupported)
                std::cout << "argument " << arg << " not supported." << endl;
#ifndef NDEBUG
            std::string start = unsupported.size() == 1 ? "if the argument (" : "if one of the arguments (";
            std::cout << start << concat_unsupported <<
            ") is actually supported, please add it to the listing in the main() function." << endl;
#endif
        }

        return unsupported.size();
    }

    optional<std::string> getValueAfterArgWithEqualsSign(const std::vector<std::string> &argNames, int argc, const char **argv) {
        for (auto &name : argNames) {
            if (auto optValue = argValueAfterEqualsSign(name, argc, argv))
                return optValue;
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
            int next = *i + 1;
            if (next < argc)
                return std::string{argv[next]};
        }
        return nullopt;
    }

}
