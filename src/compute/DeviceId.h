
#pragma once

#include <array>
#include <src/common/Optional.h>
#include <src/util/Logging.h>

namespace cl {
    class Device;
}

namespace miner {

    struct PcieIndex {
        std::array<uint8_t, 3> data;

        size_t byteSize() const;
        const uint8_t *begin() const;
        uint8_t *begin();
    };

    class DeviceId {
        PcieIndex pcieId;
    public:
        explicit DeviceId(const PcieIndex &);

        bool operator==(const DeviceId &rhs) const;
        bool operator<(const DeviceId &rhs) const;
    };

    std::vector<DeviceId> gatherAllDeviceIds();
    optional<DeviceId> obtainDeviceIdFromOpenCLDevice(cl::Device &);
}