
#include "Config.h"

#include <src/common/Json.h>
#include <src/util/Logging.h>
#include <src/util/OptionalAccess.h>
#include <src/util/StringUtils.h>

namespace miner {

    //returns json.at(key) or defaultVal
    template<class T>
    static T valueOr(const nl::json &j, const char *key, const T &defaultVal) {
        if (!j.count(key))
            return defaultVal;
        return j.at(key);
    }

    Config::Profile::Mapping parseProfileMapping(const nl::json &j) {
        return {j.at(0), j.at(1)};
    }

    void Config::parse(const nl::json &j) {
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

            pool.type = algoEnumFromString(jo.at("type"));
            pool.protocol = protoEnumFromString(jo.at("protocol"));

            if (pool.type == kAlgoTypeCount) {
                LOG(WARNING) << jo.at("type") << " is not a valid algorithm type, cannot add pool";
                continue;
            }
            if (pool.protocol == kProtoCount) {
                LOG(WARNING) << jo.at("protocol") << " is not a valid protocol type, cannot add pool";
                continue;
            }

            pool.host = jo.at("host");
            pool.port = jo.at("port");
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

                algs.core_clock_mhz_min = jo.at("core_clock_mhz_min");
                algs.core_clock_mhz_max = jo.at("core_clock_mhz_max");

                if (algs.core_clock_mhz_min > algs.core_clock_mhz_max) {
                    LOG(WARNING) << "core_clock_mhz_min is supposed to be smaller than core_clock_mhz_max";
                    algs.core_clock_mhz_max = algs.core_clock_mhz_min;
                }

                algs.memclock = jo.at("memclock");
                algs.powertune = jo.at("powertune");

                algs.num_threads = jo.at("num_threads");
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

            gs.temp_cutoff = jo.at("temp_cutoff");
            gs.temp_overheat = jo.at("temp_overheat");
            gs.temp_target = jo.at("temp_target");
            gs.temp_hysteresis = valueOr<uint32_t>(jo, "temp_hysteresis", 2);

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

            gs.opencl_kernel_dir = jo.at("opencl_kernel_dir");
            gs.start_profile = jo.at("start_profile");

            if (!getProfile(gs.start_profile)) {
                LOG(WARNING) << "specified start_profile '" << gs.start_profile << "' does not exist";
                gs.start_profile = "";
                return;
            }
        }
    }

    Config::Config(const std::string &configStr) {
        try {
            const auto j = nl::json::parse(configStr);
            tryParse(j);
        }
        catch (nl::json::exception &e) {
            LOG(ERROR) << "exception while parsing json config string: " << e.what();
        }
    }

    Config::Config(const nl::json &configJson) {
        tryParse(configJson);
    }

    void Config::tryParse(const nl::json &configJson) {
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

    optional_ref<const Config::DeviceProfile::AlgoSettings> Config::DeviceProfile::getAlgoSettings(const std::string &algoImplName) const {
        for (auto &algs : algoSettings)
            if (algs.algoImplName == algoImplName)
                return type_safe::opt_ref(algs);
        return nullopt;
    }

    optional_ref<const Config::DeviceProfile> Config::getDeviceProfile(const std::string &name) const {
        for (auto &devp : deviceProfiles)
            if (devp.name == name)
                return type_safe::opt_ref(devp);
        return nullopt;
    }

    optional_ref<Config::Profile> Config::getProfile(const std::string &name) {
        for (auto &prof : profiles)
            if (prof.name == name)
                return type_safe::opt_ref(prof);
        return nullopt;
    }

    const std::list<Config::Pool> &Config::getPools() const {
        return pools;
    }

    optional_ref<miner::Config::Profile> Config::getStartProfile() {
        return getProfile(globalSettings.start_profile);
    }

    const Config::GlobalSettings &Config::getGlobalSettings() const {
        return globalSettings;
    }

}
