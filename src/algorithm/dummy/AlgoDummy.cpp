//
//

#include "AlgoDummy.h"
#include <src/compute/ComputeModule.h>
#include <thread>
#include <src/common/Chrono.h>
#include <src/pool/WorkEthash.h>
#include <src/pool/Pool.h>
#include <src/pool/WorkDummy.h>

namespace riner {
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
                auto task = std::async(std::launch::async, [&, i = device.deviceIndex] (auto deviceThread) {
                    //deviceThread object now lives on the stack of this thread
                    SetThreadNameStream{} << "Dummy OpenCL device #" << i; //give it a name that will show up in all its log messages
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
            auto work = pool.tryGetWork<WorkDummy>();
            if (!work)
                continue; //this does not cause a busy wait loop, tryGetWork has a timeout mechanism

            LOG(INFO) << "i got mining work! (= a WorkDummy instance)";

            //get settings that have been parsed from the config file via device.settings
            auto rawIntensity = device.settings.raw_intensity;

            //... add your algorithm here ...
            sleep_for(1s);

            //report statistical data about hashrate etc via device.records.report...
            LOG(INFO) << "reporting: scanned " << rawIntensity << " nonces";
            device.records.reportScannedNoncesAmount(rawIntensity);

            sleep_for(1s);
            device.records.reportWorkUnit(work->nonce_end - work->nonce_begin, true);

            //once you notice work has expired you may abort calculation and get fresh work
            if (work->expired()) {
                continue;
            }

        }

    }

}