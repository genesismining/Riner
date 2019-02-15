#include <src/pool/WorkCuckaroo.h>

#include <lib/cl2hpp/include/cl2.hpp>
#include <src/algorithm/grin/AlgoCuckaroo31Cl.h>


namespace miner {
AlgoCuckaroo31Cl::AlgoCuckaroo31Cl(AlgoConstructionArgs args) :
        terminate_(false), args_(std::move(args)) {

    for (DeviceAlgoInfo &deviceInfo : args.assignedDevices) {
        if (optional<cl::Device> deviceOr = args.compute.getDeviceOpenCL(deviceInfo.id)) {
            cl::Device device = deviceOr.value();
            workers_.emplace_back([this, device] {
                    cl::Context context(device);
                    CuckatooSolver solver(31, context, device, args_.compute.getProgramLoaderOpenCL());
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
