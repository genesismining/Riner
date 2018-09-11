
#include "Config.h"

#include <src/common/Json.h>
#include <src/util/Logging.h>
#include <src/util/OptionalAccess.h>

namespace miner {

    Config::Config(cstring_span configStr) {
        try {
            const auto j = nl::json::parse(configStr);

            version = j.at("config_version");

            //parse pools
            for (auto &pair : asPairs(j.at("pools"))) {
                pools.insert({pair.first, Pool(pair.second)});
            }

            //parse deviceProfiles
            for (auto &pair : asPairs(j.at("device_profiles"))) {
                deviceProfiles.insert({pair.first, DeviceProfile(pair.second)});
            }

            //parse profiles
            for (auto &pair : asPairs(j.at("profiles"))) {
                profiles.insert({pair.first, Profile(pair.second, deviceProfiles)});
            }

            //parse global settings
            globalSettings = std::make_unique<GlobalSettings>(j.at("global_settings"));
        }
        catch (nl::json::exception &e) {
            LOG(ERROR) << "error while parsing json config: " << e.what();
        }
        catch (std::invalid_argument &e) {
            LOG(ERROR) << "invalid argument while parsing json config: " << e.what();
        }
        catch (std::exception &e) {
            LOG(ERROR) << "unknown exception: " << e.what();
        }

        LOG(INFO) << "parsing config string - done";
    }

    Config::GlobalSettings::GlobalSettings(const nl::json &j) {
        tempCutoff = j.at("temp_cutoff");
        tempOverheat = j.at("temp_overheat");
        tempTarget = j.at("temp_target");

        apiAllow = j.at("api_allow");
        apiEnable = j.at("api_enable");
        apiPort = j.at("api_port");;
        startProfile = j.at("start_profile");
    }

    Config::Pool::Pool(const nl::json &j) {
        type = j.at("type");
        url = j.at("url");
        username = j.at("username");
        password = j.at("password");
    }

    Config::DeviceProfile::DeviceProfile(const nl::json &j) {
        engineMin = j.at("engine_min");
        engineMax = j.at("engine_max");
        memClock = j.at("memclock");
        powertune = j.at("powertune");

        for (auto &pair : asPairs(j.at("algorithms"))) {
            algorithms.insert({pair.first, Algorithm(pair.second)});
        }
    }

    Config::DeviceProfile::Algorithm::Algorithm(const nl::json &j) {
        numThreads = j.at("num_threads");
        workSize = j.at("worksize");
        intensity = j.at("intensity");
    }

    Config::Profile::Profile(const nl::json &j,
                             std::map<std::string, Config::DeviceProfile> &deviceProfiles) {
        defaultDeviceProfile = map_at_if(deviceProfiles, j.at("default").get<std::string>());

        for (auto &pair : asPairs(j.at("gpus"))) {

            if (auto deviceProfile = map_at_if(deviceProfiles, pair.second)) {

                auto gpuIndex = std::stoul(pair.first); //may throw std::invalid_argument
                gpus.insert({gpuIndex, deviceProfile.value()});
            }
            else {
                LOG(ERROR) << "While parsing a config profile, a device_profile '" << pair.second
                           << "' was referenced that was not previously defined in 'device_profiles'";
            }
        }
    }

    optional_ref<const Config::Pool> Config::getPool(cstring_span name) const {
        return map_at_if(pools, name);
    }

    optional_ref<const Config::DeviceProfile> Config::getDeviceProfile(cstring_span name) const {
        return map_at_if(deviceProfiles, name);
    }

    optional_ref<const Config::Profile> Config::getProfile(cstring_span name) const {
        return map_at_if(profiles, name);
    }

    const Config::GlobalSettings &Config::getGlobalSettings() {
        assert(globalSettings);
        return *globalSettings;
    }
}