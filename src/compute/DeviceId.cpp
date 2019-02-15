//
//

#include "DeviceId.h"
#include <src/common/OpenCL.h>
#include <src/util/StringUtils.h>
#include <string>
#include <cstdio>

namespace miner {

    size_t PcieIndex::byteSize() const {
        return sizeof(data[0]) * data.size();
    }

    uint8_t *PcieIndex::begin() {
        return &data[0];
    }

    const uint8_t *PcieIndex::begin() const {
        return &data[0];
    }

    DeviceId::DeviceId(VendorEnum vendorEnum, const decltype(id) &idVariant, cstring_span name)
    : vendorEnum(vendorEnum)
    , id(idVariant)
    , name(gsl::to_string(name))
    {
    }

    optional_ref<const PcieIndex> DeviceId::getIfPcieIndex() const {
        return id.optional_value(type_safe::variant_type<PcieIndex>());
    }

    VendorEnum DeviceId::getVendor() const {
        return vendorEnum;
    }

    bool PcieIndex::operator==(const PcieIndex &rhs) const {
        size_t size = sizeof(data[0]) * data.size();

        int cmp = memcmp(data.data(), rhs.data.data(), size);
        return cmp == 0;
    }

    bool PcieIndex::operator<(const PcieIndex &rhs) const {
        size_t size = sizeof(data[0]) * data.size();
        int cmp = memcmp(data.data(), rhs.data.data(), size);
        return cmp < 0;
    }

    bool DeviceId::operator==(const DeviceId &rhs) const {
        //the name string is not included in this comparison
        return std::tie(vendorEnum, id) == std::tie(rhs.vendorEnum, rhs.id);
    }

    bool DeviceId::operator<(const DeviceId &rhs) const {
        //the name string is not included in this comparison
        return std::tie(vendorEnum, id) < std::tie(rhs.vendorEnum, rhs.id);
    }

    cstring_span DeviceId::getName() const {
        return name;
    }

    optional<DeviceId> obtainDeviceIdFromOpenCLDevice(cl::Device &device) {
        variant<PcieIndex, DeviceVendorId> idVariant = PcieIndex{};
        VendorEnum vendorEnum = VendorEnum::kUnknown;

        auto deviceName = device.getInfo<CL_DEVICE_NAME>();
        auto deviceVendor = device.getInfo<CL_DEVICE_VENDOR>();
        cl_int status;

        //function taken from sgminer-gm

        if (deviceVendor == "Advanced Micro Devices, Inc.") {
            vendorEnum = kAMD;
#ifndef CL_DEVICE_TOPOLOGY_TYPE_PCIE_AMD
#define CL_DEVICE_TOPOLOGY_TYPE_PCIE_AMD        1
#define CL_DEVICE_TOPOLOGY_AMD                  0x4037
            union cl_device_topology_amd {
                struct { cl_uint type; cl_uint data[5]; } raw;
                struct { cl_uint type; cl_char unused[17]; cl_char bus; cl_char device; cl_char function; } pcie;
            };
#endif
            cl_device_topology_amd topology{};
            status = device.getInfo(CL_DEVICE_TOPOLOGY_AMD, &topology);

            if (status == CL_SUCCESS && topology.raw.type == CL_DEVICE_TOPOLOGY_TYPE_PCIE_AMD) {

                PcieIndex pcieId {};
                memset(pcieId.begin(), 0xff, pcieId.byteSize());
                pcieId.data[0] = static_cast<uint8_t>(topology.pcie.bus);
                pcieId.data[1] = static_cast<uint8_t>(topology.pcie.device);
                pcieId.data[2] = static_cast<uint8_t>(topology.pcie.function);

                idVariant = pcieId;

                //printf("ComputeModule: detected PCIe topology 0000:%.2x:%.2x.%.1x\n",
                //          pcieId.data[0], pcieId.data[1], pcieId.data[2]);
            }
            else {
                return nullopt;
            }
        }
        else if (startsWith(deviceVendor, "NVIDIA")) {
            vendorEnum = kNvidia;
#ifndef CL_DEVICE_PCI_BUS_ID_NV
#define CL_DEVICE_PCI_BUS_ID_NV                     0x4008
#endif
#ifndef CL_DEVICE_PCI_SLOT_ID_NV
#define CL_DEVICE_PCI_SLOT_ID_NV                    0x4009
#endif
            cl_uint bus_id    = std::numeric_limits<cl_uint>::max();
            cl_uint device_id = std::numeric_limits<cl_uint>::max();

            status =  device.getInfo(CL_DEVICE_PCI_BUS_ID_NV, &bus_id);
            status |= device.getInfo(CL_DEVICE_PCI_SLOT_ID_NV, &device_id);

            if (status == CL_SUCCESS) {
                PcieIndex pcieId {};
                memset(pcieId.begin(), 0xff, pcieId.byteSize());
                pcieId.data[0] = static_cast<uint8_t>(bus_id);
                pcieId.data[1] = static_cast<uint8_t>(device_id);
                pcieId.data[2] = 0;
                idVariant = pcieId;
            }
            else {
                return nullopt;
            }
        }
        else if (deviceVendor == "Intel") {
            vendorEnum = kIntel;
            auto id = device.getInfo<CL_DEVICE_VENDOR_ID>();
            idVariant = id;
        }
        else {
            auto name = device.getInfo<CL_DEVICE_NAME>();
            LOG(INFO) << "could not find PCIe ID for OpenCL device '" << name << "' with vendor name '" << deviceVendor << "'";
            return nullopt;
        }

        return DeviceId(vendorEnum, idVariant, deviceName);
    };

    std::vector<DeviceId> gatherAllDeviceIds() {
        std::vector<DeviceId> result;

        std::vector<cl::Platform> clPlatforms;
        cl::Platform::get(&clPlatforms);

        if (clPlatforms.empty()) {
            LOG(INFO) << "no OpenCL platforms found when trying to obtain all device ids";
        }

        for (auto &clPlatform : clPlatforms) {
            std::vector<cl::Device> clDevices;
            clPlatform.getDevices(CL_DEVICE_TYPE_GPU, &clDevices);

            for (auto &clDevice : clDevices) {
                if (auto deviceId = obtainDeviceIdFromOpenCLDevice(clDevice)) {
                    result.push_back(deviceId.value());
                }
                else {
                    LOG(INFO) << "unable to obtain device id for clDevice at " << clDevice();
                }
            }
        }

        return result;
    }
}
