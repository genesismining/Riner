//
//

#include "AlgoDummy.h"
#include <src/compute/ComputeModule.h>
#include <thread>
#include <chrono>
#include <src/pool/WorkEthash.h>
#include <src/pool/Pool.h>

namespace miner {
    using namespace std::chrono;
    using namespace std::this_thread;

    static Algorithm::Registry<AlgoDummy> registry {"AlgoDummy", kEthash};

    AlgoDummy::AlgoDummy(AlgoConstructionArgs argsMoved)
    : _args(std::move(argsMoved)) {

        for (auto &deviceRef : _args.assignedDevices) {
            Device &device = deviceRef.get();

            if (auto clDeviceOr = _args.compute.getDeviceOpenCL(device.id)) {
                //prepare thread objects
                _deviceThreads.emplace_back(DeviceThread{
                    *this,
                    _args.workProvider,
                    device,
                    std::move(clDeviceOr.value())
                });
            }
        }

        //launch threads
        for (auto &thread : _deviceThreads) {
            thread.task = std::async(std::launch::async, [&] () {
                thread.run();
            });
        }
    }

    AlgoDummy::~AlgoDummy() {
        _shutdown = true;
        //implicitly joins threads in std::future destructor
    }

    void AlgoDummy::DeviceThread::run() {
        sleep_for(milliseconds(200 * device.deviceIndex));

        while (!algo._shutdown) {

            auto workOr = pool.tryGetWork<kEthash>().value_or(nullptr);
            if (!workOr)
                continue;

            Work<kEthash> &work = *workOr;

            auto rawIntensity = device.settings.raw_intensity;

            device.records.reportAmtTraversedNonces(rawIntensity);
            sleep_for(seconds(1));

            device.records.reportFailedShareVerification();
            sleep_for(seconds(1));

            if (work.expired()) {
                continue;
            }
        }

    }

}