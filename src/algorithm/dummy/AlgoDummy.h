#pragma once

#include <src/algorithm/Algorithm.h>
#include <vector>
#include <future>
#include <atomic>
#include <src/common/OpenCL.h>
#include <src/util/LockUtils.h>

namespace miner {

    class AlgoDummy : public Algorithm {

        AlgoConstructionArgs _args;

        std::atomic_bool _shutdown {false};

        struct DeviceThread { //contains everything a device thread needs in our algorithm
            AlgoDummy &algo;
            Pool &pool;
            Device &device;
            cl::Device clDevice;

            void run();
        };

        std::list<std::future<void>> tasks;

    public:
        explicit AlgoDummy(AlgoConstructionArgs args);

        ~AlgoDummy() override;

    };

}