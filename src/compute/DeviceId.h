
#pragma once

#include <array>
#include <src/common/Optional.h>
#include <src/common/Variant.h>
#include <src/util/Logging.h>
#include <src/compute/ComputeApiEnums.h>

namespace cl {
    class Device;
}

namespace riner {

    struct PcieIndex {
        uint16_t segment = 0;
        uint8_t bus = 0;
        uint8_t device = 0xff;
        uint8_t function = 0;

        bool operator==(const PcieIndex &rhs) const;
        bool operator<(const PcieIndex &rhs) const;

        inline auto tie() const {
            return std::tie(segment, bus, device, function);
        }
    };

    using DeviceVendorId = uint32_t; //DeviceVendorId is used as a fallback for devices where detecting the PcieIndex fails. it may not work as a cross-api unique identifier

    class DeviceId {
        VendorEnum vendorEnum = VendorEnum::kUnknown;
        variant<PcieIndex, DeviceVendorId> id;
        std::string name; //name is not used in comparison operators

    public:
        DeviceId(VendorEnum vendorEnum, const decltype(id) &idVariant, std::string name);
        DeviceId(DeviceId &&)                 = default;
        DeviceId &operator=(DeviceId &&)      = default;
        DeviceId(const DeviceId &)            = default;
        DeviceId &operator=(const DeviceId &) = default;

        VendorEnum getVendor() const;
        const std::string & getName() const;
        optional_cref<PcieIndex> getIfPcieIndex() const;

        bool operator==(const DeviceId &rhs) const;
        bool operator<(const DeviceId &rhs) const;
    };

    std::vector<DeviceId> gatherAllDeviceIds();
    optional<DeviceId> obtainDeviceIdFromOpenCLDevice(cl::Device &);
}