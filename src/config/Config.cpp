
#include "Config.h"

#include <src/pool/Pool.h>
#include <src/util/Logging.h>
#include <src/util/OptionalAccess.h>
#include <src/util/StringUtils.h>

#include <google/protobuf/text_format.h>
#include <google/protobuf/util/message_differencer.h>
#include <src/config/ConfigDefaultValues.h>

namespace riner {

    optional<Config> parseConfig(const std::string &txtProto) {
        bool success = true;

        using TextFormat = google::protobuf::TextFormat;

        Config config;
        success = TextFormat::ParseFromString(defaultConfigCStr, &config);
        if (!success) {
            LOG(ERROR) << "error while parsing default config values (not the user provided config)";
            return nullopt;
        }

        success = TextFormat::MergeFromString(txtProto, &config);
        if (!success) {
            LOG(INFO) << "error while parsing config";
            return nullopt;
        }

        if (validateConfig(config)) {
            return config;
        }
        return nullopt;
    }

    bool validateConfig(const Config &c) {
        //e.g. check whether keys in settings_for_algoimpl are actual AlgoImpl names of registered classes
        //TODO: implement
        return true;
    }

    optional_cref<proto::Config_Profile> getStartProfile(const Config &c) {

        if (c.has_global_settings() && c.global_settings().has_start_profile_name()) {
            for (size_t i = 0; i < c.profile_size(); ++i) {
                auto &prof = c.profile(i);

                if (prof.name() == c.global_settings().start_profile_name()) {
                    return prof;
                }
            }
        }
        return nullopt;
    }

    optional_cref<proto::Config_DeviceProfile> getDeviceProfile(const Config &c, const std::string &devProfileName) {

        for (size_t i = 0; i < c.device_profile_size(); ++i) {
            auto &devProf = c.device_profile(i);

            if (devProf.name() == devProfileName) {
                return devProf;
            }
        }
        return nullopt;
    }

    optional<AlgoSettings> getAlgoSettings(const proto::Config_DeviceProfile &dp, const std::string &algoImplName) {
        auto &settMap = dp.settings_for_algoimpl();
        auto it = settMap.find(algoImplName);
        if (it != settMap.end()) {
            return AlgoSettings{it->second, algoImplName};
        }
        return nullopt;
    }


    AlgoSettings::AlgoSettings(const proto::Config_DeviceProfile_AlgoSettings &p, const std::string &algoImplNameArg) {

#define set_if_has(x, y) if (p.has_##y ()) x = p. y ()

        // // example:
        // set_if_has(gpu.memory_clock_MHz, memory_clock_mhz);
        // // expands to:
        // if (p.has_memory_clock_mhz())
        //     gpuSettings.memory_clock_MHz = p.memory_clock_mhz();

        algoImplName = algoImplNameArg;
        set_if_has(gpuSettings.memory_clock_MHz, memory_clock_mhz);
        set_if_has(gpuSettings.core_clock_MHz_min, core_clock_mhz_min);
        set_if_has(gpuSettings.core_clock_MHz_max, core_clock_mhz_max);
        set_if_has(gpuSettings.core_clock_MHz, core_clock_mhz);
        set_if_has(gpuSettings.memory_clock_MHz, memory_clock_mhz);
        set_if_has(gpuSettings.power_limit_W, power_limit_w);
        set_if_has(gpuSettings.power_limit_percent, power_limit_percent);
        set_if_has(gpuSettings.core_voltage_mV, core_voltage_mv);
        set_if_has(gpuSettings.core_voltage_offset_mV, core_voltage_offset_mv);

        set_if_has(num_threads, num_threads);
        set_if_has(work_size, work_size);
        set_if_has(raw_intensity, raw_intensity);

#undef set_if_has
    }

}
