

#include "ConfigUtils.h"
#include "FileUtils.h"
#include <set>
#include <deque>

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
    getConfigPoolsForPowType(const Config &config, const std::string &algoName) {
        std::vector<std::reference_wrapper<const Config::Pool>> result;

        for (auto &pool : config.getPools()) {
            if (pool.powType == algoName)
                result.emplace_back(std::cref(pool));
        }

        return result;
    }

    std::vector<std::reference_wrapper<Device>> prepareAssignedDevicesForAlgoImplName(const std::string &implName,
            const Config &config, Config::Profile &prof, std::deque<optional<Device>> &devicesInUse, const std::vector<DeviceId> &deviceIds) {
        std::vector<std::reference_wrapper<Device>> result;

        for (size_t i = 0; i < deviceIds.size(); ++i) {
            auto mapping = getMappingForDevice(prof, i); //e.g. ["DeviceProfile", "AlgoEthashCL"]
            auto &deviceId = deviceIds[i];

            if (mapping.algoImplName == implName) {
                //this device is supposed to be used by this algoImpl

                if (devicesInUse[i]) {
                    LOG(WARNING) << "device #" << i << " (" << gsl::to_string(deviceId.getName()) << ") cannot be used for '" << implName << "' because it is already used by another active algorithm";
                    continue;
                }

                optional<Device> &deviceToInit = devicesInUse[i];

                auto &devProf = config.getDeviceProfile(mapping.deviceProfileName).value();

                if (auto algoSettings = devProf.getAlgoSettings(implName)) {

                    //initialize the device inside the devicesInUse list, and put it
                    //into the assignedDevices list of this algoImpl

                    deviceToInit.emplace(deviceId, algoSettings.value(), i);
                    result.emplace_back(deviceToInit.value());
                }
                else {
                    LOG(WARNING) << "no algorithm settings for '" << implName << "' in deviceProfile '" << devProf.name << "'";
                    continue;
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