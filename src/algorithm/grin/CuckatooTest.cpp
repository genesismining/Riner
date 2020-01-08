#include <src/algorithm/grin/Cuckatoo.h>

#include <src/algorithm/grin/AlgoCuckatoo31Cl.h>
#include <src/common/Optional.h>
#include <src/compute/DeviceId.h>
#include <src/pool/WorkCuckoo.h>
#include <src/util/HexString.h>
#include <src/util/Logging.h>
#include <src/util/StringUtils.h>
#include <src/util/FileUtils.h>
#include <src/util/TaskExecutorPool.h>

#include <gtest/gtest.h>
#include <gmock/gmock.h>

#include <algorithm>

namespace miner {
namespace {

constexpr bool kEnableOpenClTests = true;

TEST(Siphash, GenerateUV) {
    SiphashKeys keys;
    keys.k0 = 12144847460615431484ULL;
    keys.k1 =  4704460813036456867ULL;
    keys.k2 = 16388201774304843670ULL;
    keys.k3 = 10268448706773269360ULL;

    const uint32_t edge = 5985112;
    const uint32_t nodemask = (1 << 29) - 1;
    EXPECT_EQ(404521472, siphash24(&keys, 2 * edge + 0) & nodemask);
    EXPECT_EQ(342813478, siphash24(&keys, 2 * edge + 1) & nodemask);
}

class CuckatooSolverTest: public testing::Test {
    constexpr static const char *testKernelDir = "./src/";
public:

    CuckatooSolverTest() :
            programLoader(testKernelDir, "") {
        header = std::make_unique<WorkCuckatoo31>();
    }

protected:
    std::unique_ptr<CuckatooSolver> createSolver(uint32_t n) {
        cl_int err;
        device = cl::Device::getDefault(&err);
        if (!kEnableOpenClTests || err) {
            LOG(WARNING)<< "Failed to obtain a OpenCL device";
            return nullptr;
        }

        optional<DeviceId> deviceId = obtainDeviceIdFromOpenCLDevice(device);
        EXPECT_TRUE(deviceId.has_value());
        LOG(INFO) << "Running on device " << deviceId->getName();
        vendor = deviceId->getVendor();
        context = cl::Context(device);

        AlgoSettings algoSettings;
        Device algoDevice = {*deviceId, algoSettings, 0};

        CuckatooSolver::Options options {algoDevice, tasks};
        options.programLoader = &programLoader;
        options.n = n;
        options.context = context;
        options.device = device;

        std::vector<std::string> files = {"/kernel/siphash.h", "/kernel/cuckatoo.cl"};
        std::string fileDir = testKernelDir;
        for (auto &fileName : files) {
            auto path = fileDir + fileName;
            if (!file::readFileIntoString(path)) {
                LOG(WARNING)<< "Failed to load one of the required kernel files";
                return nullptr;
            }
        }

        return std::make_unique<CuckatooSolver>(std::move(options));
    }

    CLProgramLoader programLoader;
    cl::Device device;
    cl::Context context;
    VendorEnum vendor = VendorEnum::kUnknown;
    std::unique_ptr<WorkCuckatoo31> header;
    TaskExecutorPool tasks {1};
};

TEST_F(CuckatooSolverTest, Solve29) {
    std::unique_ptr<CuckatooSolver> solver = createSolver(29);
    if (solver == nullptr) {
        LOG(WARNING)<< "Failed to obtain a OpenCL device. Skipping test";
        return;
    }

    header->prePow = {0x41,0x42,0x43};
    header->prePow.resize(72, 0);
    header->nonce = 0x15000000;

    solver->solve(AlgoCuckatoo31Cl::calculateKeys(*header),
            [] (std::vector<CuckatooSolver::Cycle> cycles) {
                ASSERT_EQ(1, cycles.size());
                EXPECT_THAT(cycles[0].edges, testing::ElementsAreArray( {
                    31512508, 59367126, 60931190, 94763886, 104277898, 116747030, 127554684,
                    142281893, 197249170, 210206965, 211338509, 256596889, 259030601, 261857131, 268667508, 271769895,
                    295284253, 296568689, 319619493, 324904830, 338819144, 340659072, 385650715, 385995656, 392428799,
                    406828082, 411433229, 412126883, 430148237, 435642922, 451290256, 451616065, 467529516, 472555386,
                    474712317, 503536255, 504936349, 509088607, 510814466, 519326390, 521564062, 525046456 }));
            },
            [] {return false;})
            .wait();
}

TEST_F(CuckatooSolverTest, Solve31) {
    std::unique_ptr<CuckatooSolver> solver = createSolver(31);
    if (solver == nullptr) {
        LOG(WARNING)<< "Failed to create CuckatooSolver. Skipping test";
        return;
    }

    std::string hex = "0001000000000000ce54000000005c6e92a4000002cf90d4ed85c43063baf5c681ac054309a3719464d8cf4d6d2e2b38f51144ef86059dda0c73a68bce94407ab7d28b381c52a108659684336749e16f0786cabf1d575b97a7b9ad7e5306f3feb328216d62d581f1fcaee49222b7cf60436748e5abe6ecbbed054c05532b4b9afdd9fe3e03041cfa7cbab5d40866f42df910132647982234aa306a3fd628088b17a3053be72991dd4c0d6a9e8183657e4c39ff530a7f06436b8d99df6c069a182dd53166870aa2c4ae44f5ce28d8f2754aef00000000000f26ad000000000005fde3000062c5c4f056ea00000545";
    header->nonce=8742930641540181280ULL;
	HexString h(hex);
    header->prePow.resize(h.sizeBytes());
    h.getBytes(header->prePow);
    SiphashKeys keys = AlgoCuckatoo31Cl::calculateKeys(*header);    
    // Siphash Keys: 707696558862008831, 13844509301656340219, 10878251467021832460, 1593815210236709481

    solver->solve(
            keys,
            [&keys] (std::vector<CuckatooSolver::Cycle> cycles) {
                ASSERT_EQ(1, cycles.size());
                LOG(INFO) << "Edges: " << toString(cycles[0].edges);
                EXPECT_TRUE(CuckatooSolver::isValidCycle(31, 42, keys, cycles[0]));
                EXPECT_THAT(cycles[0].edges, testing::ElementsAreArray(
                        {25658671, 33517224, 169257816, 263176471, 275158520, 279314901, 304448673, 325368254,
                         460577821, 523175179, 562671497, 563415534, 658916799, 748246646, 760653191, 841797550,
                         884140246, 1055282821, 1071033665, 1103276156, 1144618546, 1145968364, 1185166942, 1355190350,
                         1394097284, 1443271097, 1519523172, 1640431487, 1670430783, 1680011930, 1703111873, 1716961047,
                         1729277548, 1751151239, 1755525050, 1779819427, 1789289757, 1833144376, 2038031226, 2046215501,
                         2111816174, 2115207736}));
            },
            [] {return false;})
            .wait();
}

TEST_F(CuckatooSolverTest, IsValidCycle) {
    header->prePow = {0x41,0x42,0x43};
    header->prePow.resize(72, 0);
    header->nonce = 0x15000000;
    SiphashKeys keys = AlgoCuckatoo31Cl::calculateKeys(*header);

    CuckatooSolver::Cycle cycle;
    cycle.edges = { 31512508, 59367126, 60931190, 94763886, 104277898, 116747030, 127554684,
                    142281893, 197249170, 210206965, 211338509, 256596889, 259030601, 261857131, 268667508, 271769895,
                    295284253, 296568689, 319619493, 324904830, 338819144, 340659072, 385650715, 385995656, 392428799,
                    406828082, 411433229, 412126883, 430148237, 435642922, 451290256, 451616065, 467529516, 472555386,
                    474712317, 503536255, 504936349, 509088607, 510814466, 519326390, 521564062, 525046456 };
    EXPECT_TRUE(CuckatooSolver::isValidCycle(29, 42, keys, cycle));

    CuckatooSolver::Cycle noCycle = cycle;
    std::swap(noCycle.edges[0], noCycle.edges[6]);
    EXPECT_FALSE(CuckatooSolver::isValidCycle(29, 42, keys, noCycle));

    noCycle = cycle;
    noCycle.edges[7]++;
    EXPECT_FALSE(CuckatooSolver::isValidCycle(29, 42, keys, noCycle));

    noCycle = cycle;
    for(int i=0; i<42; ++i) {
        noCycle.edges.push_back(noCycle.edges[41] + i + 1);
    }
    EXPECT_FALSE(CuckatooSolver::isValidCycle(29, 2*42, keys, noCycle));
}

}
 // namespace
}// miner
