#include <src/algorithm/grin/Cuckatoo.h>

#include <src/algorithm/grin/AlgoCuckatoo31Cl.h>
#include <src/common/Optional.h>
#include <src/compute/DeviceId.h>
#include <src/pool/WorkCuckaroo.h>
#include <src/util/HexString.h>
#include <src/util/Logging.h>
#include <src/util/StringUtils.h>

#include <gtest/gtest.h>
#include <gmock/gmock.h>

#include <algorithm>

namespace miner {
namespace {

class CuckatooSolverTest: public testing::Test {
public:
    CuckatooSolverTest() :
            programLoader("src/algorithm/", "") {
        cl_int err;
        device = cl::Device::getDefault(&err);
        if (err) {
            LOG(WARNING)<< "Failed to obtain a OpenCL device. Skipping test";
            return;
        }
        skip = false;

        optional<DeviceId> deviceInfo = obtainDeviceIdFromOpenCLDevice(device);
        EXPECT_TRUE(deviceInfo.has_value());
        LOG(INFO)<< "Running on device " << gsl::to_string(deviceInfo.value().getName());
        vendor = deviceInfo.value().getVendor();
        context = cl::Context(device);
        workProtocolData = std::make_shared<WorkProtocolData>(33);
        header = std::make_unique<CuckooHeader>(workProtocolData);
    }

protected:
    std::unique_ptr<CuckatooSolver> createSolver(uint32_t n) {
        CuckatooSolver::Options options(programLoader);
        options.n = n;
        options.context = context;
        options.device = device;
        options.vendor = vendor;
        return std::make_unique<CuckatooSolver>(std::move(options));
    }

    bool skip = true;
    CLProgramLoader programLoader;
    cl::Device device;
    cl::Context context;
    VendorEnum vendor;
    std::shared_ptr<WorkProtocolData> workProtocolData;
    std::unique_ptr<CuckooHeader> header;
};

TEST_F(CuckatooSolverTest, Solve29) {
    if (skip) {
        return;
    }
    header->prePow = {0x41,0x42,0x43};
    header->prePow.resize(72, 0);
    header->nonce = static_cast<uint64_t>(21) << 32;

    std::unique_ptr<CuckatooSolver> solver = createSolver(29);
    std::vector<CuckatooSolver::Cycle> cycles = solver->solve(AlgoCuckatoo31Cl::calculateKeys(*header), [] {return false;});
    ASSERT_EQ(1, cycles.size());
    EXPECT_THAT(cycles[0].edges,
            testing::ElementsAreArray( { 31512508, 59367126, 60931190, 94763886, 104277898, 116747030, 127554684,
                    142281893, 197249170, 210206965, 211338509, 256596889, 259030601, 261857131, 268667508, 271769895,
                    295284253, 296568689, 319619493, 324904830, 338819144, 340659072, 385650715, 385995656, 392428799,
                    406828082, 411433229, 412126883, 430148237, 435642922, 451290256, 451616065, 467529516, 472555386,
                    474712317, 503536255, 504936349, 509088607, 510814466, 519326390, 521564062, 525046456 }));
}

TEST_F(CuckatooSolverTest, Solve31) {
    if (skip) {
        return;
    }
    std::string hex = "0001000000000000bfdd000000005c6b3879000001b02fb55c5795c91f6887707e2b50d7d27dbe0d186429f52c37d2e3420a44d26e8a20308d9de3266d01efc9fc325839bfc9b931c11b6e15febabd5babe3df3b6de0ee393493842a7b2d30640af6e1a251fe6e530bcb063d185cc5037479357438a7d7ad5d8d912df096d64721bc736c5349e7697ba8027d0cc54868c96e8750661b993aa6cb3a6c736d8b2028dce2bb378740cf3f36f381069047d35e83f4221ed2f3386c64806f1022d4cdb842963b2b0ed326a46229b648e17fdaf27600000000000e65de00000000000596d80000551b70b1eb16000003ba";
    HexString h(hex);
    header->prePow.resize(h.sizeBytes());
    h.getBytes(header->prePow);
    header->nonce = 2;
    // Siphash Keys: 13495520118231073436, 9682857131365348831, 17644065428756129924, 4450569698308320556

    workProtocolData = nullptr;
    std::unique_ptr<CuckatooSolver> solver = createSolver(31);
    std::vector<CuckatooSolver::Cycle> cycles = solver->solve(AlgoCuckatoo31Cl::calculateKeys(*header), [] {return false;});
    ASSERT_EQ(1, cycles.size());
    EXPECT_THAT(cycles[0].edges,
            testing::ElementsAreArray( {
                    40778812, 45624768, 48317588, 66386056, 100584285, 236190618,
                    246446957, 286867101, 296430628, 308121924, 318525521, 534437425,
                    574947317, 701022016, 726150994, 763011423, 777629599, 809282263,
                    869636888, 937277103, 938751072, 1097494066, 1119540307, 1164059537,
                    1215435736, 1258075186, 1296647487, 1307948060, 1341542971, 1390228227,
                    1441771255, 1443696283, 1475085213, 1616615237, 1621535967, 1735854578,
                    1829645800, 1959427709, 1989573565, 2044045378, 2081416868, 2126250393 }));
}

} // namespace
} // miner
