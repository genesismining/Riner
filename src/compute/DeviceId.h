
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
        std::array<uint8_t, 3> data;

        size_t byteSize() const;
        const uint8_t *begin() const;
        uint8_t *begin();

        bool operator==(const PcieIndex &rhs) const;
        bool operator<(const PcieIndex &rhs) const;
    };

    using DeviceVendorId = uint32_t; //DeviceVendorId is used as a fallback for devices where detecting the PcieIndex fails. it may not work as a cross-api unique identifier

    class DeviceId {
        VendorEnum vendorEnum = VendorEnum::kUnknown;
        variant<PcieIndex, DeviceVendorId> id;
        std::string name; //name is not used in comparison operators

    public:
        DeviceId(VendorEnum vendor, const decltype(id) &id, cstring_span name);

        VendorEnum getVendor() const;
        cstring_span getName() const;

        bool operator==(const DeviceId &rhs) const;
        bool operator<(const DeviceId &rhs) const;
    };

    std::vector<DeviceId> gatherAllDeviceIds();
    optional<DeviceId> obtainDeviceIdFromOpenCLDevice(cl::Device &);
}