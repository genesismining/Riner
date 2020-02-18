
#include "Device.h"

namespace miner {

    Device::Device(const DeviceId &_id, const AlgoSettings &_settings, size_t _deviceIndex)
    : id(_id)
    , settings(_settings)
    , deviceIndex(_deviceIndex)
    , api(GpuApi::tryCreate(GpuApiConstructionArgs{_id, _settings.gpuSettings})) {
    }

}