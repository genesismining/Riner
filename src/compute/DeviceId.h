
#pragma once

#include <array>
#include <src/common/Optional.h>
#include <src/common/Variant.h>
#include <src/common/StringSpan.h>
#include <src/util/Logging.h>
#include <src/compute/ComputeApiEnums.h>

namespace cl {
    class Device;
}

namespace miner {

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
        mp::variant<PcieIndex, DeviceVendorId> id;
        std::string name; //name is not used in comparison operators

    public:
        DeviceId(VendorEnum vendor, const decltype(id) &id, cstring_span name);
        DeviceId(DeviceId &&)                 = default;
        DeviceId &operator=(DeviceId &&)      = default;
        DeviceId(const DeviceId &)            = default;
        DeviceId &operator=(const DeviceId &) = default;

        VendorEnum getVendor() const;
        cstring_span getName() const;
        optional_cref<PcieIndex> getIfPcieIndex() const;

        bool operator==(const DeviceId &rhs) const;
        bool operator<(const DeviceId &rhs) const;
    };

    std::vector<DeviceId> gatherAllDeviceIds();
    opt::optional<DeviceId> obtainDeviceIdFromOpenCLDevice(cl::Device &);
}