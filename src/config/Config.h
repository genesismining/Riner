#pragma once

#include <list>
#include <string>
#include <src/common/Pointers.h>
#include <src/common/Optional.h>

#include <config.pb.h> //protobuf generated header for LegacyConfig.proto

namespace riner {

    using Config = proto::Config;

    /**
     * takes the textproto string in google protocol buffer textProto format and tries to parse it according to `Config.proto`
     * Parser errors are shown in the logs.
     * return a valid config object or nullopt if parsing failed.
     */
    optional<Config> parseConfig(const std::string &txtProto);

    /**
     * fills a given `config` with default values if they weren't specified in the textproto file.
     * If you want to add default values, edit this function's implementation
     */
    void fillInDefaultValuesIfNeeded(Config &config);

    /**
     * performs basic sanitization of a given config. These checks still allow for some inconsistencies, so that e.g. there can be mismatching names as long as they aren't actually used by riner. This is to make config editing more convenient.
     * The remaining errors will get caught by `Applicaton` once a config profile is actually started
     */
    bool validateConfig(const Config &);

    /**
     * return the config profile which the application should be started with
     */
    optional_cref<proto::Config_Profile> getStartProfile(const Config &);

    /**
     * finds the device profile with name `devProfileName` in `c`
     * return reference to the device profile or nullopt if it couldn't be found.
     */
    optional_cref<proto::Config_DeviceProfile> getDeviceProfile(const Config &c, const std::string &devProfileName);

    /**
     * Settings struct that the Device is populated with according to config info, once a profile is started
     */
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

    /**
     * Algorithm settings struct that the Device is populated with according to device profile info, once a profile is started
     */
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

    /**
     * wrapper around AlgoSettings constructor that can fail if no Settings were found for that algoImplName in the device profile
     * return found AlgoSettings or nullopt
     */
    optional<AlgoSettings> getAlgoSettings(const proto::Config_DeviceProfile &dp, const std::string &algoImplName);

}
