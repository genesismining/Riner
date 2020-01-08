
#include "Config.h"

#include <src/pool/Pool.h>
#include <src/common/Json.h>
#include <src/util/Logging.h>
#include <src/util/OptionalAccess.h>
#include <src/util/StringUtils.h>

#include <google/protobuf/text_format.h>
#include <google/protobuf/util/message_differencer.h>
#include <src/config/ConfigDefaultValues.h>

#define PARSE_OPTIONAL(settings, json, memberName) optionalValue(settings.memberName, json, #memberName)

namespace miner {

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

        return config;
    }

    optional_cref<proto::Config_Profile> getStartProfile(Config &c) {

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

    optional_cref<proto::Config_DeviceProfile> getDeviceProfile(Config &c, const std::string &devProfileName) {

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

        //example:
        // set_if_has(gpu.memory_clock_MHz, memory_clock_mhz);
        // expands to:
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



    namespace {

    template<typename T>
    static inline void optionalValue(T &ret, const nl::json &j, const char *key) {
        const auto &it = j.find(key);
        if (it == j.end())
            ret = {};
        else
            ret.emplace(*it);
    }

    template<typename T>
    static inline optional<T> optionalValue(const nl::json &j, const char *key) {
        optional<T> ret;
        optionalValue(ret, j, key);
        return ret;
    }

    //returns json.at(key) or defaultVal
    template<class T>
    static inline T valueOr(const nl::json &j, const char *key, const T &defaultVal) {
        const auto &it = j.find(key);
        if (it == j.end())
            return defaultVal;
        return *it;
    }

    LegacyConfig::Profile::Mapping parseProfileMapping(const nl::json &j) {
        if (j.size() != 2) {
            LOG(ERROR) << "profile mapping must be [DeviceProfile, AlgoImplName]";
        }
        return {j.at(0), j.at(1)};
    }

    }


    void LegacyConfig::parse(const nl::json &j) {
        using namespace std::string_literals;

        std::string version = j.at("config_version");
        const auto supported = "1.0"s;
        if (version != supported) {
            LOG(ERROR) << "config has unsupported version (" << version
                       << "). only " << supported << " is supported";
            return;
        }

        //parse pools
        for (auto &jo : j.at("pools")) {
            Pool pool;

            pool.protocolType = jo.at("protocol");
            pool.powType = miner::Pool::getPowTypeForPoolImplName(pool.protocolType);
            // check whether protocolType is actually a poolImplName
            if (!pool.powType.empty())
                pool.protocolType = miner::Pool::getProtocolTypeForPoolImplName(pool.protocolType);
            else {
                if (!miner::Pool::hasProtocolType(pool.protocolType)) {
                    LOG(WARNING) << pool.protocolType << " is not a valid protocol type, cannot add pool";
                    continue;
                }

                pool.powType = jo.at("type");
                if (!miner::Pool::hasPowType(pool.powType)) {
                    LOG(WARNING) << pool.powType << " is not a valid POW type, cannot add pool";
                    continue;
                }
            }

            pool.host = jo.at("host");

            {
                auto port64 = jo.at("port").get<int64_t>();
                if (port64 < 0 || port64 > std::numeric_limits<uint16_t>::max()) {
                    LOG(WARNING) << "port " << port64 << " specified in config is outside of valid range";
                }
                pool.port = gsl::narrow_cast<uint16_t>(port64);
            }

            pool.username = jo.at("username");
            pool.password = jo.at("password");

            pools.push_back(pool);
        }

        //parse device profile
        for (auto &pair : asPairs(j.at("device_profiles"))) {
            DeviceProfile devp;

            auto &jo = pair.second;

            devp.name = pair.first;

            //parse algorithm settings for this device profile
            for (auto &pair : asPairs(jo.at("algorithms"))) {
                DeviceProfile::AlgoSettings algs;

                auto &jo = pair.second;

                algs.algoImplName = pair.first;
                auto &gpuSettings = algs.gpuSettings;

                PARSE_OPTIONAL(gpuSettings, jo, core_clock_MHz_min);
                PARSE_OPTIONAL(gpuSettings, jo, core_clock_MHz_max);
                PARSE_OPTIONAL(gpuSettings, jo, core_clock_MHz);

                if (gpuSettings.core_clock_MHz_min.value_or(0) > gpuSettings.core_clock_MHz_max.value_or(UINT32_MAX)) {
                    LOG(WARNING) << "core_clock_mhz_min is supposed to be smaller than core_clock_mhz_max";
                    gpuSettings.core_clock_MHz_max = gpuSettings.core_clock_MHz_min;
                }

                PARSE_OPTIONAL(gpuSettings, jo, memory_clock_MHz);
                PARSE_OPTIONAL(gpuSettings, jo, power_limit_W);
                PARSE_OPTIONAL(gpuSettings, jo, core_voltage_mV);
                PARSE_OPTIONAL(gpuSettings, jo, core_voltage_offset_mV);

                algs.num_threads = valueOr(jo, "num_threads", uint32_t(1));
                algs.work_size = jo.at("work_size");
                algs.raw_intensity = jo.at("raw_intensity");

                if (algs.raw_intensity == 0) {
                    LOG(WARNING) << "raw_intensity must not be 0";
                    LOG(WARNING) << "cannot add algo settings for '" << algs.algoImplName << "' to device profile '" << devp.name << "'";
                    continue;
                }

                devp.algoSettings.push_back(algs);
            }

            deviceProfiles.push_back(devp);
        }

        //parse profiles
        for (auto &pair : asPairs(j.at("profiles"))) {
            Profile prof;

            auto &jo = pair.second;

            prof.name = pair.first;
            prof.device_default = parseProfileMapping(jo.at("device_default"));

            if (j.count("devices_by_index")) {
                for (auto &pair : asPairs(j.at("devices_by_index"))) {
                    auto index = static_cast<size_t>(std::stoll(pair.first));
                    auto mapping = parseProfileMapping(pair.second);

                    if (!getDeviceProfile(mapping.deviceProfileName)) {
                        LOG(WARNING) << "device profile '" << mapping.deviceProfileName << "' requested for device with index " << index << " does not exist";
                        continue;
                    }

                    prof.devices_by_index[index] = mapping;
                }
            }

            profiles.push_back(prof);
        }

        //parse global settings
        {
            auto &jo = j.at("global_settings");
            auto &gs = globalSettings;

            PARSE_OPTIONAL(gs, jo, temp_cutoff);
            PARSE_OPTIONAL(gs, jo, temp_overheat);
            PARSE_OPTIONAL(gs, jo, temp_target);
            gs.temp_hysteresis = valueOr<uint32_t>(jo, "temp_hysteresis", 3);

            if (!(gs.temp_target <= gs.temp_overheat && gs.temp_overheat <= gs.temp_cutoff)) {
                LOG(WARNING) << "temperature parameters don't satisfy 'temp_target <= temp_overheat <= temp_cutoff'";
                gs.temp_cutoff = gs.temp_overheat = gs.temp_target; //TODO: come up with something better here
            }

            uint16_t apiPortDefault = 4028;
            auto apiPort_i64 = valueOr<int64_t>(jo, "api_port", apiPortDefault);
            if (apiPort_i64 >= 0 && apiPort_i64 < std::numeric_limits<decltype(gs.api_port)>::max()) {
                gs.api_port = apiPort_i64;
            }
            else {
                LOG(INFO) << "specified api port (" << apiPort_i64 << ") is outside of the valid range, using default port " << apiPortDefault << " instead";
                gs.api_port = apiPortDefault;
            }

            std::string default_opencl_kernel_dir("./");
            gs.opencl_kernel_dir = valueOr(jo, "opencl_kernel_dir", default_opencl_kernel_dir);
            if (gs.opencl_kernel_dir.empty())
                gs.opencl_kernel_dir = default_opencl_kernel_dir;
            gs.start_profile = jo.at("start_profile");

            if (!getProfile(gs.start_profile)) {
                LOG(WARNING) << "specified start_profile '" << gs.start_profile << "' does not exist";
                gs.start_profile = "";
                return;
            }
        }
    }

    LegacyConfig::LegacyConfig(const std::string &configStr) {
        try {
            const auto j = nl::json::parse(configStr);
            tryParse(j);
        }
        catch (nl::json::exception &e) {
            LOG(ERROR) << "exception while parsing json config string: " << e.what();
        }
    }

    LegacyConfig::LegacyConfig(const nl::json &configJson) {
        tryParse(configJson);
    }

    void LegacyConfig::tryParse(const nl::json &configJson) {
        try {
            valid = false;
            parse(configJson);
            valid = true;
            LOG(INFO) << "parsing config string - done";
        }
        catch (nl::json::exception &e) {
            LOG(ERROR) << "exception while parsing json config: " << e.what();
        }
        catch (std::invalid_argument &e) {
            LOG(ERROR) << "invalid argument while parsing json config: " << e.what();
        }
        catch (std::exception &e) {
            LOG(ERROR) << "unknown exception while parsing json config: " << e.what();
        }
    }

    optional_cref<LegacyConfig::DeviceProfile::AlgoSettings> LegacyConfig::DeviceProfile::getAlgoSettings(
            const std::string &algoImplName) const {
        for (auto &algs : algoSettings)
            if (algs.algoImplName == algoImplName)
                return algs;
        return {};
    }

    optional_cref<LegacyConfig::DeviceProfile> LegacyConfig::getDeviceProfile(const std::string &name) const {
        for (auto &devp : deviceProfiles)
            if (devp.name == name)
                return devp;
        return {};
    }

    optional_ref<LegacyConfig::Profile> LegacyConfig::getProfile(const std::string &name) {
        for (auto &prof : profiles)
            if (prof.name == name)
                return prof;
        return {};
    }

    const std::list<LegacyConfig::Pool> &LegacyConfig::getPools() const {
        return pools;
    }

    optional_ref<LegacyConfig::Profile> LegacyConfig::getStartProfile() {
        return getProfile(globalSettings.start_profile);
    }

    const LegacyConfig::GlobalSettings &LegacyConfig::getGlobalSettings() const {
        return globalSettings;
    }

}
