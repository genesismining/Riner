
#pragma once

#include <src/config/Config.h>
#include <src/compute/DeviceId.h>
#include <src/application/Device.h>
#include <vector>
#include <deque>

namespace miner {

    namespace configUtils {

        optional<Config> loadConfig(const std::string &configPath);

        std::vector<std::string> getUniqueAlgoImplNamesForProfile(const proto::Config_Profile &prof, const std::vector<DeviceId> &deviceIds);

        std::vector<std::reference_wrapper<const proto::Config_Pool>> getConfigPoolsForPowType(const proto::Config &config,
                                                                                         const std::string &powType);

        optional_cref<proto::Config_Profile_Task> getTaskForDevice(const proto::Config_Profile &prof, size_t deviceIndex);

        std::vector<std::reference_wrapper<Device>> prepareAssignedDevicesForAlgoImplName(const std::string &implName, const Config &config, const proto::Config_Profile &prof, std::deque<optional<Device>> &devicesInUse, const std::vector<DeviceId> &allIds);

    }
}