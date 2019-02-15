#include <src/pool/WorkCuckaroo.h>

#include <lib/cl2hpp/include/cl2.hpp>
#include <src/algorithm/grin/AlgoCuckaroo31Cl.h>


namespace miner {
AlgoCuckaroo31Cl::AlgoCuckaroo31Cl(AlgoConstructionArgs args) :
        terminate_(false), args_(std::move(args)) {

    for (DeviceAlgoInfo &deviceInfo : args_.assignedDevices) {
        if (optional<cl::Device> deviceOr = args.compute.getDeviceOpenCL(deviceInfo.id)) {
            cl::Device device = deviceOr.value();
            workers_.emplace_back([this, deviceInfo, device] {
                    cl::Context context(device);
                    CuckatooSolver::Options opts(args_.compute.getProgramLoaderOpenCL());
                    opts.n = 31;
                    opts.context = context;
                    opts.device = device;
                    opts.vendor = deviceInfo.id.getVendor();

                    CuckatooSolver solver(std::move(opts));
                    run(context, solver);
                });

        }
    }
}

AlgoCuckaroo31Cl::~AlgoCuckaroo31Cl() {
    terminate_ = true;
    for(auto& worker: workers_) {
        worker.join();
    }
}

void AlgoCuckaroo31Cl::run(cl::Context& context, CuckatooSolver& solver) {
    while(!terminate_) {
        unique_ptr<CuckooHeader> work = args_.workProvider.tryGetWork<kCuckaroo31>().value_or(nullptr);
        if (!work) {
            // TODO maybe sleep some
            continue;
        }

        std::unique_ptr<CuckooSolution> solution = solver.solve(std::move(work));
    }
}

} /* namespace miner */
