#pragma once

#include <src/compute/DeviceId.h>
#include <src/gpu_api/GpuApi.h>
#include <src/statistics/DeviceRecords.h>
#include <src/util/Copy.h>
#include "Config.h"

namespace miner {

    struct Device {//device interface for usage by Algorithms
        using AlgoSettings = Config::DeviceProfile::AlgoSettings;

        Device(const DeviceId &_id, const AlgoSettings &_settings, size_t _deviceIndex);
        Device(Device &&)                 = default;
        Device &operator=(Device &&)      = default;

        DELETE_COPY(Device);

        const DeviceId id;
        const AlgoSettings settings;
        const size_t deviceIndex = std::numeric_limits<size_t>::max();

        DeviceRecords records; //report per-device hashrate etc, via this object
        std::unique_ptr<GpuApi> api;
    };

}