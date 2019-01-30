

#include "ConfigUtils.h"
#include "FileUtils.h"
#include <set>

namespace miner {namespace configUtils {

    Config loadConfig(miner::cstring_span configPath) {

        auto configStr = file::readFileIntoString(configPath);
        if (!configStr) {
            LOG(ERROR) << "unable to read config file";
            return {};
        }

        LOG(INFO) << "parsing config string:\n" << configStr.value();
        return Config{configStr.value()};
    }

    std::vector<std::string>
    getUniqueAlgoImplNamesForProfile(Config::Profile &prof, const std::vector<DeviceId> &deviceIds) {

        std::set<std::string> set; //implicitly removes duplicates

        for (size_t i = 0; i < deviceIds.size(); ++i)
            set.insert(getMappingForDevice(prof, i).algoImplName);

        return {set.begin(), set.end()};
    }

    std::vector<std::reference_wrapper<const Config::Pool>>
    getConfigPoolsForAlgoType(const Config &config, AlgoEnum algoType) {
        std::vector<std::reference_wrapper<const Config::Pool>> result;

        for (auto &pool : config.getPools()) {
            if (pool.type == algoType)
                result.emplace_back(std::cref(pool));
        }

        return result;
    }

    std::vector<DeviceAlgoInfo> getAllDeviceAlgoInfosForAlgoImplName(const std::string &implName,
            const Config &config, Config::Profile &prof, const std::vector<DeviceId> &deviceIds) {

        std::vector<DeviceAlgoInfo> result;

        for (size_t i = 0; i < deviceIds.size(); ++i) {
            auto mapping = getMappingForDevice(prof, i);
            auto &deviceId = deviceIds[i];

            if (mapping.algoImplName == implName) {

                auto &devProf = config.getDeviceProfile(mapping.deviceProfileName).value();

                if (auto algoSettings = devProf.getAlgoSettings(implName)) {
                    result.emplace_back(DeviceAlgoInfo{algoSettings.value(), deviceId, i});
                }
                else {
                    LOG(WARNING) << "no algorithm settings for '" << implName << "' in deviceProfile '" << devProf.name << "'";
                }
            }
        }
        return result;
    }


    Config::Profile::Mapping getMappingForDevice(Config::Profile &prof, size_t deviceIndex) {
        if (prof.devices_by_index.count(deviceIndex)) {
            return prof.devices_by_index.at(deviceIndex);
        }
        return prof.device_default;
    }

}}