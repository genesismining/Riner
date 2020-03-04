//
//

#include <src/util/Logging.h>
#include "CLI.h"

#include "gtest/gtest.h"

int main(int raw_argc, char **raw_argv) {
    using namespace riner;

    auto a = copyArgsAndExpandSingleDashCombinedArgs(raw_argc, (const char **)raw_argv);
    bool use_log_colors = hasArg({"--color", "--colors"}, a.argc, a.argv);
    bool use_log_emojis = hasArg({"--emoji", "--emojis"}, a.argc, a.argv);

    ::testing::InitGoogleTest(&raw_argc, raw_argv);
    initLogging(a.argc, a.argv, use_log_colors, use_log_emojis);
    setThreadName("test main");
    return RUN_ALL_TESTS();
}