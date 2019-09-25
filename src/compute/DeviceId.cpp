//
//

#include "DeviceId.h"
#include <src/common/OpenCL.h>
#include <string>
#include <cstdio>

namespace miner {

    DeviceId::DeviceId(VendorEnum vendorEnum, const decltype(id) &idVariant, cstring_span name)
    : vendorEnum(vendorEnum)
    , id(idVariant)
    , name(gsl::to_string(name))
    {
    }

    optional_cref<PcieIndex> DeviceId::getIfPcieIndex() const {
        if (auto ptr = var::get_if<PcieIndex>(&id)) {
            return *ptr;
        }
        return {};
    }

    VendorEnum DeviceId::getVendor() const {
        return vendorEnum;
    }

    bool PcieIndex::operator==(const PcieIndex &rhs) const {
        return tie() == rhs.tie();
    }

    bool PcieIndex::operator<(const PcieIndex &rhs) const {
        return tie() < rhs.tie();
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
                pcieId.bus = static_cast<uint8_t>(topology.pcie.bus);
                pcieId.device = static_cast<uint8_t>(topology.pcie.device);
                pcieId.function = static_cast<uint8_t>(topology.pcie.function);

                idVariant = pcieId;

                //printf("ComputeModule: detected PCIe topology %4x:%.2x:%.2x.%.1x\n",
                //          pcieId.segment, pcieId.bus, pcieId.device, pcieId.function);
            }
            else {
                return nullopt;
            }
        }
        else if (deviceVendor.substr(0, 6) == "NVIDIA") {
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
                pcieId.bus = static_cast<uint8_t>(bus_id);
                pcieId.device = static_cast<uint8_t>(device_id);
                idVariant = pcieId;
            }
            else {
                return nullopt;
            }
        }
        else if (deviceVendor.substr(0, 5) == "Intel") {
            vendorEnum = kIntel;
            auto id = device.getInfo<CL_DEVICE_VENDOR_ID>();
            idVariant = id; // TODO: use unique identifier
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
                    result.push_back(*deviceId);
                }
                else {
                    LOG(INFO) << "unable to obtain device id for clDevice at " << clDevice();
                }
            }
        }

        return result;
    }
}