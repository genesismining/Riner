#pragma once

#include <map>
#include <string>
#include <src/common/StringSpan.h>
#include <src/common/Pointers.h>
#include <src/common/Optional.h>
#include <src/common/JsonForward.h>

namespace miner {

    class Config { //TODO: sanitize values before usage
    public:
        template<class T>
        using ref = std::reference_wrapper<T>;

        explicit Config(cstring_span configStr);

        struct GlobalSettings {
            uint32_t tempCutoff = 85;
            uint32_t tempOverheat = 80;
            uint32_t tempTarget = 76;

            std::string apiAllow;
            bool apiEnable;
            uint32_t apiPort;
            std::string startProfile;

            explicit GlobalSettings(const nl::json &jsonObject);
        };

        struct Pool {
            std::string type;
            std::string url;
            std::string username;
            std::string password;

            explicit Pool(const nl::json &jsonObject);
        };

        struct DeviceProfile {
            uint32_t engineMin = 0;
            uint32_t engineMax = 0;
            uint32_t memClock = 0;
            uint32_t powertune = 0;

            struct Algorithm {
                std::string name;

                uint32_t numThreads = 0;
                uint32_t workSize = 0;
                uint32_t intensity = 0;

                explicit Algorithm(const nl::json &jsonObject);
            };

            std::map<std::string, Algorithm> algorithms;

            explicit DeviceProfile(const nl::json &jsonObject);
        };

        struct Profile {
            optional_ref<DeviceProfile> defaultDeviceProfile;
            std::map<size_t, ref<DeviceProfile>> gpus;

            Profile(const nl::json &jsonObject,
                    std::map<std::string, DeviceProfile> &deviceProfiles);
        };

        optional_ref<const Pool> getPool(cstring_span name) const;
        optional_ref<const DeviceProfile> getDeviceProfile(cstring_span name) const;
        optional_ref<const Profile> getProfile(cstring_span name) const;
        const GlobalSettings &getGlobalSettings();

        template<class Func>
        void forEachPool(const Func &func) {
            for (auto &pool : pools) {
                func(pool.second);
            }
        }

    private:

        std::string version;
        unique_ptr<GlobalSettings> globalSettings;

        std::map<std::string, Pool> pools;
        std::map<std::string, DeviceProfile> deviceProfiles;
        std::map<std::string, Profile> profiles;
    };

}
