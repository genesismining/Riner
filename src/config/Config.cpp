
#include "Config.h"

#include <src/pool/Pool.h>
#include <src/util/Logging.h>
#include <src/util/OptionalAccess.h>
#include <src/util/StringUtils.h>
#include <src/application/Registry.h>

#include <google/protobuf/text_format.h>
#include <google/protobuf/util/message_differencer.h>
#include <google/protobuf/util/field_mask_util.h>

namespace riner {

    optional<Config> parseConfig(const std::string &txtProto) {
        bool success = true;

        using TextFormat = google::protobuf::TextFormat;

        Config config;
        try {
        success = TextFormat::ParseFromString(txtProto, &config);
        }
        catch(std::exception &e) {
            LOG(ERROR) << "std except in parse from string: "<< e.what();
        }

        if (!success) {
            LOG(ERROR) << "error while parsing config";
            return nullopt;
        }

        fillInDefaultValuesIfNeeded(config);

        if (validateConfig(config)) {
            return config;
        }
        return nullopt;
    }


#define default_value(obj, member, value) \
if (!obj. has_##member () ) { \
    VLOG(3) << "config entry '" << #member << "' defaulted to " << #value; \
    obj.set_##member(value); \
}

    void fillInDefaultValuesIfNeeded(Config &config) {
        Config::GlobalSettings &gs = *config.mutable_global_settings(); //this call instantiates if needed.

        //global_settings default values
        default_value(gs, api_port, 4028);

        //device_profiles
        for (auto &dp : *config.mutable_device_profile()) {
            for (auto &pair : *dp.mutable_settings_for_algoimpl()) {
                Config::DeviceProfile::AlgoSettings &as = pair.second;

                //algo_settings default values

                if (as.has_power_limit_w()) {
                    //no defaults
                }
                else if (as.has_power_limit_percent()) {
                    default_value(as, power_limit_percent, 100);
                }

                default_value(as, num_threads, 1);
                default_value(as, work_size, 128);
                default_value(as, raw_intensity, 1048576);
            }
        }

        //pool default values
        for (auto &p : *config.mutable_pool()) {
            (void)p;
            //no defaults
        }
    }

#undef default_value

#define check_between(x, _min, _max) \
if (!(_min <= x && x <= _max)) { \
    throw std::invalid_argument(MakeStr{} << "config value '" << #x \
    << "'= " << x << " out of valid range [" << _min << ", " << _max << "]"); \
}

    bool validateConfig(const Config &config) {
        constexpr uint32_t u32_max = std::numeric_limits<uint32_t>::max();

        Registry registry;

        //e.g. check whether keys in settings_for_algoimpl are actual AlgoImpl names of registered classes
        const Config::GlobalSettings &gs = config.global_settings();

        try {
            //global_settings default values
            check_between(gs.api_port(), 1, 65535);

            //device_profiles
            for (auto &dp : config.device_profile()) {
                for (auto &pair : dp.settings_for_algoimpl()) {
                    std::string algoImplName = pair.first;
                    if (!registry.algoImplExists(algoImplName)) {
                        LOG(WARNING) << "device profile '" << dp.name() << "' has settings for unknown AlgoImpl: '" << algoImplName << "'";
                    }

                    const Config::DeviceProfile::AlgoSettings &as = pair.second;

                    //algo_settings default values
                    if (as.has_power_limit_w()) {
                        //no limits
                    }
                    if (as.has_power_limit_percent()) {
                        check_between(as.power_limit_percent(), 0, 200);
                    }

                    check_between(as.num_threads(), 1, u32_max);
                    check_between(as.work_size(), 128, u32_max);
                    if (as.work_size() > 1024) {
                        LOG(WARNING) << "unusually high work_size of " << as.work_size() << " found. proceeding.";
                    }

                    check_between(as.raw_intensity(), 1, u32_max)//TODO: values

                }
            }
        }
        catch (const std::invalid_argument &e) {
            LOG(ERROR) << "invalid config: " << e.what();
            return false;
        }

        return true;
    }

#undef check_between

    optional_cref<proto::Config_Profile> getStartProfile(const Config &c) {
        if (c.global_settings().has_start_profile_name()) {
            for (auto &prof : c.profile()) {
                if (prof.name() == c.global_settings().start_profile_name()) {
                    return prof;
                }
            }
        }
        return nullopt;
    }

    optional_cref<proto::Config_DeviceProfile> getDeviceProfile(const Config &c, const std::string &devProfileName) {

        for (auto &devProf : c.device_profile()) {
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
