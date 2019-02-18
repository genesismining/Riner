#include <src/algorithm/grin/Cuckatoo.h>

#include <src/pool/WorkCuckaroo.h>
#include <src/util/Logging.h>
#include <src/common/Optional.h>

#include <gtest/gtest.h>
#include <gmock/gmock.h>

#include <algorithm>

namespace miner {
namespace {

TEST(CuckatooSolver, Solve29) {
    cl::Device device = cl::Device::getDefault();
    LOG(INFO) << "Running on device " << device.getInfo<CL_DEVICE_NAME>();
    cl::Context context(device);

    CLProgramLoader programLoader("src/algorithm/", "");
    CuckatooSolver::Options options(programLoader);
    options.n = 29;
    options.context = context;
    options.device = device;
    options.vendor = VendorEnum::kAMD; // FIXME

    CuckatooSolver solver(std::move(options));

    auto p = std::make_shared<WorkProtocolData>(33);
    std::weak_ptr<WorkProtocolData> wp = p;
    auto header = std::make_unique<CuckooHeader>(wp);
    header->prePow = {0x41,0x42,0x43};
    header->prePow.resize(72, 0);
    header->nonce = static_cast<uint64_t>(21) << 32;

    std::vector<CuckatooSolver::Cycle> cycles = solver.solve(std::move(header));
    ASSERT_EQ(1, cycles.size());

    EXPECT_THAT(cycles[0].edges, testing::ElementsAreArray( {
        31512508, 59367126, 60931190, 94763886, 104277898, 116747030, 127554684,
        142281893, 197249170, 210206965, 211338509, 256596889, 259030601, 261857131,
        268667508, 271769895, 295284253, 296568689, 319619493, 324904830, 338819144,
        340659072, 385650715, 385995656, 392428799, 406828082, 411433229, 412126883,
        430148237, 435642922, 451290256, 451616065, 467529516, 472555386, 474712317,
        503536255, 504936349, 509088607, 510814466, 519326390, 521564062, 525046456}));
}



} // namespace
} // miner
