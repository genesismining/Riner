#pragma once

#include <src/compute/DeviceId.h>
#include <src/gpu_api/GpuApi.h>
#include <src/statistics/DeviceRecords.h>
#include <src/util/Copy.h>
#include "src/config/Config.h"

namespace riner {

    /**
     * device bundle for usage by `AlgoImpls`
     */
    struct Device {//device interface for usage by Algorithms

        /**
         * create a `Device` for passing device related information and handles to `AlgoImpl`s via `AlgoConstructionArgs`
         * param _id the generic DeviceId of the Device that is supposed to be represented
         * param _settings the AlgoSettings for this device under the currently running device profile/AlgoImpl combination
         * param _deviceIndex the index of the device's id in `ComputeModule::getAllDeviceIds()`
         */
        Device(const DeviceId &_id, const AlgoSettings &_settings, size_t _deviceIndex);
        Device(Device &&) = default;

        DELETE_COPY(Device);

        const DeviceId id;
        const AlgoSettings settings;
        const size_t deviceIndex = std::numeric_limits<size_t>::max();

        /**
         * report per-device hashrate etc, via this object from within an `AlgoImpl`
         */
        DeviceRecords records;
        
        /**
         * interact with an optional GpuApi. May be nullptr if no GpuApi could be initialized.
         */
        std::unique_ptr<GpuApi> api;
    };

}
