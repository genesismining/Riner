#pragma once

#include <list>
#include <string>
#include <src/common/Pointers.h>
#include <src/common/Optional.h>

#include <config.pb.h> //protobuf generated header for LegacyConfig.proto

namespace riner {

    using Config = proto::Config;
    optional<Config> parseConfig(const std::string &txtProto);
    void fillInDefaultValuesIfNeeded(Config &config);
    bool validateConfig(const Config &);

    optional_cref<proto::Config_Profile> getStartProfile(const Config &);
    optional_cref<proto::Config_DeviceProfile> getDeviceProfile(const Config &c, const std::string &devProfileName);

    struct GpuSettings {
        optional<uint32_t>
                core_clock_MHz,
                memory_clock_MHz,
                power_limit_W, //power limit is either expressed in watts or percentage
                power_limit_percent,
                core_voltage_mV;
        optional<int32_t>
                core_voltage_offset_mV;
    };

    struct AlgoSettings {
        AlgoSettings() = default;
        AlgoSettings(const proto::Config_DeviceProfile_AlgoSettings &, const std::string &algoImplName);

        std::string algoImplName;

        GpuSettings gpuSettings;

        uint32_t
                num_threads = 0,
                work_size = 0,
                raw_intensity = 0;
    };

    optional<AlgoSettings> getAlgoSettings(const proto::Config_DeviceProfile &dp, const std::string &algoImplName);

}
