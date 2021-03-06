

#include "ConfigUtils.h"
#include "FileUtils.h"
#include <set>
#include <deque>

namespace riner {namespace configUtils {

    optional<Config> loadConfig(const std::string &configPath) {

        auto configStr = file::readFileIntoString(configPath);
        if (!configStr) {
            LOG(ERROR) << "unable to read config file";
            return {};
        }

        VLOG(0) << "parsing config string";
        VLOG(3) << "config string:\n" << configStr.value();
        return parseConfig(configStr.value());
    }

    std::vector<std::string>
    getUniqueAlgoImplNamesForProfile(const proto::Config_Profile &prof, const std::vector<DeviceId> &deviceIds) {

        std::set<std::string> s; //implicitly removes duplicates

        for (size_t i = 0; i < deviceIds.size(); ++i) {
            if (auto taskOr = getTaskForDevice(prof, i)) {
                s.insert(taskOr->run_algoimpl_with_name());
            }
        }

        return {s.begin(), s.end()};
    }

    std::vector<std::reference_wrapper<const proto::Config_Pool>> getConfigPoolsForPowType(const proto::Config &config, const std::string &powType) {
        std::vector<std::reference_wrapper<const proto::Config_Pool>> result;

        for (int i = 0; i < config.pool_size(); ++i) {
            auto &pool = config.pool(i);
            if (powType == pool.pow_type())
                result.emplace_back(pool);
        }

        return result;
    }

    std::vector<std::reference_wrapper<Device>> prepareAssignedDevicesForAlgoImplName(const std::string &implName,
            const Config &config, const proto::Config_Profile &prof, std::deque<optional<Device>> &devicesInUse, const std::vector<DeviceId> &deviceIds) {
        std::vector<std::reference_wrapper<Device>> result;

        for (size_t i = 0; i < deviceIds.size(); ++i) {
            auto taskOr = getTaskForDevice(prof, i);
            if (!taskOr)
                continue;

            auto &task = *taskOr;
            auto &deviceId = deviceIds[i];

            if (task.run_algoimpl_with_name() == implName) {
                //this device is supposed to be used by this algoImpl

                if (devicesInUse[i]) {
                    LOG(WARNING) << "device #" << i << " (" << deviceId.getName() << ") cannot be used for '" << implName << "' because it is already used by another active algorithm";
                    continue;
                }

                auto devProfOr = getDeviceProfile(config, task.use_device_profile_with_name());
                if (!devProfOr) {
                    LOG(WARNING) << "no device profile with name '" << task.use_device_profile_with_name() << "'. Skipping device #" << i << " (" << deviceId.getName() << ")";
                    continue;
                }

                auto algoSettingsOr = getAlgoSettings(*devProfOr, implName);
                if (!algoSettingsOr) {
                    LOG(WARNING) << "no algorithm settings for '" << implName << "' in device #" << i << " '" << deviceId.getName() << "'´s device profile '" << devProfOr->name() << "'. Device #" << i << " will not be used.";
                    continue;
                }

                //initialize the device inside the devicesInUse list, and put it
                //into the assignedDevices list of this algoImpl

                optional<Device> &deviceToInit = devicesInUse[i];
                deviceToInit.emplace(deviceId, *algoSettingsOr, i);
                result.emplace_back(*deviceToInit);

            }
        }
        return result;
    }

    optional_cref<proto::Config_Profile_Task> getTaskForDevice(const proto::Config_Profile &prof, size_t deviceIndex) {

        optional<size_t> chosen_task_i;
        for (int i = 0; i < prof.task_size(); ++i) {
            auto &task = prof.task(i);

            if (task.has_run_on_all_remaining_devices() && task.run_on_all_remaining_devices()) {
                chosen_task_i = i; //write it to result now, it may be returned later unless this device gets another assignment somewhere else
            }

            if (task.has_device_index() && task.device_index() == deviceIndex) {
                chosen_task_i = i;
                break;
            }

            if (task.has_device_alias_name()) {
                LOG(WARNING) << "device aliases are not supported yet, ignoring task #" << i;
            }
        }

        if (chosen_task_i) {
            auto i = *chosen_task_i;
            VLOG(2) << "assigning device #" << deviceIndex << " to profile '" << prof.name() << "'´s task #" << i << " (counting from #0)";
            return prof.task(i);
        }
        else {
            VLOG(2) << "assigning no task to device #" << deviceIndex << " under profile '" << prof.name() << "'";
            return nullopt;
        }
    }

}}
