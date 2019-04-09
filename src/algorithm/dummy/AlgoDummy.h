#pragma once

#include <src/algorithm/Algorithm.h>
#include <vector>
#include <future>
#include <atomic>
#include <src/common/OpenCL.h>
#include <src/util/LockUtils.h>

namespace miner {

    class AlgoDummy : public AlgoBase {

        AlgoConstructionArgs _args;

        std::atomic_bool _shutdown {false};

        struct DeviceThread {
            AlgoDummy &algo;
            WorkProvider &pool;
            Device &device;
            cl::Device clDevice;

            void run();
            std::future<void> task;
        };
        friend DeviceThread;

        std::vector<DeviceThread> _deviceThreads;

    public:
        explicit AlgoDummy(AlgoConstructionArgs args);

        ~AlgoDummy() override;

    };

}