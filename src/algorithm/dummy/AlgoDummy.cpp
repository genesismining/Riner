//
//

#include "AlgoDummy.h"
#include <src/compute/ComputeModule.h>
#include <thread>
#include <src/common/Chrono.h>
#include <src/pool/WorkEthash.h>
#include <src/pool/Pool.h>

namespace miner {
    using namespace std::chrono;
    using namespace std::chrono_literals;
    using namespace std::this_thread;

    AlgoDummy::AlgoDummy(AlgoConstructionArgs argsMoved)
    : _args(std::move(argsMoved)) {

        //assignedDevices contains all the compute devices the algo should use
        for (auto &deviceRef : _args.assignedDevices) {
            auto &device = deviceRef.get();

            //try to obtain the OpenCL device handle for the assigned device via its id
            if (optional<cl::Device> clDeviceOr = _args.compute.getDeviceOpenCL(device.id)) {

                //prepare everything the device thread is going to need
                DeviceThread deviceThread = {
                        *this,
                        _args.workProvider,
                        device,
                        std::move(*clDeviceOr)
                };

                //start a new thread
                auto task = std::async(std::launch::async, [&] (auto deviceThread) {
                    //deviceThread object now lives on the stack of this thread
                    deviceThread.run();
                }, std::move(deviceThread));

                tasks.push_back(std::move(task)); //collect tasks.
                // since every task is moved into `tasks`, the tasks dtor will act as a thread-join in AlgoDummy's dtor
            }
        }
    }

    AlgoDummy::~AlgoDummy() {
        _shutdown = true;
        //implicitly joins threads in 'tasks' std::future destructor
    }

    void AlgoDummy::DeviceThread::run() {

        sleep_for(200ms * device.deviceIndex); //offset devices by 200ms

        //run until dtor tells us to shut down
        while (!algo._shutdown) {

            //get work from pool or try again
            auto work = pool.tryGetWork<WorkEthash>();
            if (!work)
                continue; //this does not cause a busy wait loop, tryGetWork has a timeout mechanism

            //get settings that have been parsed from the config file via device.settings
            auto rawIntensity = device.settings.raw_intensity;

            //... add algorithm here ...

            //report statistical data about hashrate etc via device.records.report...
            device.records.reportScannedNoncesAmount(rawIntensity);
            sleep_for(1s);

            device.records.reportWorkUnit(work->deviceDifficulty, true);
            sleep_for(1s);

            //once you notice work has expired you may abort calculation and get fresh work
            if (work->expired()) {
                continue;
            }

        }

    }

}