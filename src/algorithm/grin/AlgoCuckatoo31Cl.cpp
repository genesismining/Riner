#include "AlgoCuckatoo31Cl.h"

#include <src/common/Endian.h>
#include <src/common/OpenCL.h>
#include <src/crypto/blake2.h>
#include <src/pool/WorkCuckoo.h>

namespace riner {

AlgoCuckatoo31Cl::AlgoCuckatoo31Cl(AlgoConstructionArgs args) :
        terminate_(false), args_(std::move(args)) {

    for (auto &assignedDeviceRef : args_.assignedDevices) {
        auto &assignedDevice = assignedDeviceRef.get();

        optional<cl::Device> deviceOr = args.compute.getDeviceOpenCL(assignedDevice.id);
        if (!deviceOr) {
            continue;
        }
        cl::Device device = *deviceOr;
        workers_.emplace_back([this, &assignedDevice, device] {
            cl::Context context(device);
            CuckatooSolver::Options opts {assignedDevice, tasks};
            opts.n = 31;
            opts.context = context;
            opts.device = device;
            opts.programLoader = &args_.compute.getProgramLoaderOpenCL();

            CuckatooSolver solver(std::move(opts));
            run(context, solver);
        });
    }
}

AlgoCuckatoo31Cl::~AlgoCuckatoo31Cl() {
    terminate_ = true;
    for (auto& worker : workers_) {
        worker.join();
    }
}

/* static */SiphashKeys AlgoCuckatoo31Cl::calculateKeys(const WorkCuckatoo31& header) {
    SiphashKeys keys;
    std::vector<uint8_t> h = header.prePow;
    uint64_t nonce = header.nonce;
    VLOG(0) << "nonce = " << nonce;
    size_t len = h.size();
    h.resize(len + sizeof(nonce));
    *reinterpret_cast<uint64_t*>(&h[len]) = htobe64(nonce);
    uint64_t keyArray[4];
    blake2b(keyArray, sizeof(keyArray), h.data(), h.size(), 0, 0);
    keys.k0 = htole64(keyArray[0]);
    keys.k1 = htole64(keyArray[1]);
    keys.k2 = htole64(keyArray[2]);
    keys.k3 = htole64(keyArray[3]);
    return keys;
}

void AlgoCuckatoo31Cl::run(cl::Context& context, CuckatooSolver& solver) {
    while (!terminate_) {
        auto work = std::shared_ptr<WorkCuckatoo31>(
                args_.workProvider.tryGetWork<WorkCuckatoo31>());
        if (!work) {
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
            continue;
        }

        solver.solve(
                calculateKeys(*work),
                [&, work](std::vector<CuckatooSolver::Cycle> cycles) {
                    solver.getDevice().records.reportScannedNoncesAmount(1);
                    LOG(INFO)<< "Found " << cycles.size() << " cycles of target length.";
                    // TODO sha pow and compare to difficulty
                    for (auto& cycle : cycles) {
                        solver.getDevice().records.reportWorkUnit(1., true);
                        auto pow = work->makeWorkSolution<WorkSolutionCuckatoo31>();
                        pow->nonce = work->nonce;
                        pow->pow = std::move(cycle.edges);
                        args_.workProvider.submitSolution(std::move(pow));
                    }
                },
                [this, work] {
                    return !work->valid() || terminate_.load(std::memory_order_relaxed);
                });
    }
}

} /* namespace miner */
