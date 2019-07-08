#include <src/pool/Work.h>

#include <src/pool/WorkCuckoo.h>

#include <src/util/Logging.h>
#include <gtest/gtest.h>
#include <gmock/gmock.h>

#include <algorithm>

namespace miner {
namespace {

TEST(Work, CreateResult) {
    auto wpd = std::make_shared<WorkProtocolData>(88);
    Work<kCuckatoo31> work(wpd);

    unique_ptr<WorkResult<kCuckatoo31>> result = work.makeWorkResult<kCuckatoo31>();
    EXPECT_FALSE(work.expired());
    EXPECT_FALSE(result->tryGetProtocolDataAs<WorkProtocolData>() == nullptr);
}

TEST(Work, CreateResultFromExpiredWork) {
    auto wpd = std::make_shared<WorkProtocolData>(88);
    Work<kCuckatoo31> work(wpd);
    wpd.reset();
    EXPECT_TRUE(work.expired());

    unique_ptr<WorkResult<kCuckatoo31>> result = work.makeWorkResult<kCuckatoo31>();
    EXPECT_TRUE(result->tryGetProtocolDataAs<WorkProtocolData>() == nullptr);
}


} // namespace
} // miner

