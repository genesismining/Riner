#include <src/pool/Work.h>

#include <src/pool/WorkCuckoo.h>
#define private public
#include <src/pool/PoolGrin.h>
#undef private
#include <src/statistics/PoolRecords.h>

#include <src/util/Logging.h>
#include <gtest/gtest.h>
#include <gmock/gmock.h>

#include <algorithm>

namespace miner {
namespace {

class GrinJobs : public testing::Test {

private:
    PoolConstructionArgs args{"", 0, "", ""};

protected:
    std::shared_ptr<PoolGrinStratum> pool;
    std::unique_ptr<WorkCuckatoo31> invalidWork;
    std::unique_ptr<WorkCuckatoo31> oldWork;
    std::unique_ptr<WorkCuckatoo31> latestWork;
    std::unique_ptr<WorkSolutionCuckatoo31> invalidSolution;
    std::unique_ptr<WorkSolutionCuckatoo31> oldSolution;
    std::unique_ptr<WorkSolutionCuckatoo31> latestSolution;

    GrinJobs() {
        auto typeErasedPool = Pool::makePool(args, "Cuckatoo31Stratum");
        pool = std::static_pointer_cast<PoolGrinStratum>(typeErasedPool);

        nl::json job = {{"height", 10}, {"job_id", 2}, {"difficulty", 1}, {"pre_pow", "00"}};
        pool->onMiningNotify(job);
        invalidWork = std::move(pool->tryGetWork<WorkCuckatoo31>().value());
        invalidSolution = std::move(invalidWork->makeWorkSolution<WorkSolutionCuckatoo31>());

        job["height"] = 11;
        job["job_id"] = 0;
        pool->onMiningNotify(job);
        oldWork = std::move(pool->tryGetWork<WorkCuckatoo31>().value());
        oldSolution = std::move(oldWork->makeWorkSolution<WorkSolutionCuckatoo31>());

        job["job_id"] = 1;
        pool->onMiningNotify(job);
        latestWork = std::move(pool->tryGetWork<WorkCuckatoo31>().value());
        latestSolution = std::move(latestWork->makeWorkSolution<WorkSolutionCuckatoo31>());
    }
};

TEST_F(GrinJobs, ExpiryWorks) {

    using limits = std::numeric_limits<double>;
    EXPECT_EQ(2. + limits::epsilon(), 2.);
    EXPECT_NE(1. + limits::epsilon(), 1.);
    EXPECT_NE(1. - limits::epsilon() / limits::radix, 1.);

    EXPECT_NE(pool, nullptr);

    EXPECT_FALSE(latestWork->expired());
    EXPECT_FALSE(latestSolution->expired());
    EXPECT_TRUE(latestWork->valid());
    EXPECT_TRUE(latestSolution->valid());

    EXPECT_TRUE(oldWork->expired());
    EXPECT_TRUE(oldSolution->expired());
    EXPECT_TRUE(oldWork->valid());
    EXPECT_TRUE(oldSolution->valid());

    EXPECT_TRUE(invalidWork->expired());
    EXPECT_TRUE(invalidSolution->expired());
    EXPECT_FALSE(invalidWork->valid());
    EXPECT_FALSE(invalidSolution->valid());

}

TEST_F(GrinJobs, PoolDeletionWorks) {
    pool = nullptr;

    EXPECT_TRUE(latestWork->expired());
    EXPECT_TRUE(latestSolution->expired());
    EXPECT_FALSE(latestWork->valid());
    EXPECT_FALSE(latestSolution->valid());
}


} // namespace
} // miner

