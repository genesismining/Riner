//
//

#include <src/util/Logging.h>

#include "gtest/gtest.h"

int main(int argc, char **argv) {
    using namespace riner;

    ::testing::InitGoogleTest(&argc, argv);
    initLogging(argc, (const char **)argv);
    setThreadName("test main");
    return RUN_ALL_TESTS();
}