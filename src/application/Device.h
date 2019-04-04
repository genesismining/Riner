#pragma once

#include <src/compute/DeviceId.h>
#include <src/statistics/DeviceRecords.h>
#include "Config.h"

namespace miner {

    struct Device {//device interface for usage by Algorithms
        using AlgoSettings = Config::DeviceProfile::AlgoSettings;

        Device(const Device &)            = delete;
        Device &operator=(const Device &) = delete;
        Device(Device &&)                 = default;
        Device &operator=(Device &&)      = default;

        const DeviceId id;
        const AlgoSettings settings;
        const size_t deviceIndex = std::numeric_limits<size_t>::max();

        DeviceRecords records; //report per-device hashrate etc, via this object
    };

}