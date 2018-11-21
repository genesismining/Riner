
#include <iostream>
#include "Application.h"
#include <src/common/Optional.h>

#include <src/util/Logging.h>
INITIALIZE_EASYLOGGINGPP

namespace miner {

    //this function is used to get a config path until we have a proper argc argv parser
    optional<std::string> getPathAfterArg(cstring_span minusminusArg, int argc, const char *argv[]) {
        //minusminusArg is something like "--config"
        size_t pathI = 0;
        for (size_t i = 1; i < argc; ++i) {
            if (gsl::ensure_z(argv[i]) == minusminusArg) {
                pathI = i + 1; //path starts at next arg
                break;
            }
        }
        if (pathI == 0)
            return nullopt;

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

}

int main(int argc, const char *argv[]) {
    using namespace miner;
    START_EASYLOGGINGPP(argc, argv);

    auto configPath = getPathAfterArg("--config", argc, argv);

    miner::Application app(configPath);
    return 0;
}