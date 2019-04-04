
#pragma once

#include <src/application/Config.h>
#include <src/compute/DeviceId.h>
#include <src/application/Device.h>
#include <vector>

namespace miner {

    namespace configUtils {

        Config loadConfig(cstring_span configPath);

        std::vector<std::string> getUniqueAlgoImplNamesForProfile(Config::Profile &prof, const std::vector<DeviceId> &deviceIds);

        std::vector<std::reference_wrapper<const Config::Pool>> getConfigPoolsForAlgoType(const Config &config, AlgoEnum algoType);

        Config::Profile::Mapping getMappingForDevice(Config::Profile &prof, size_t deviceIndex);

        std::vector<std::reference_wrapper<Device>> prepareAssignedDevicesForAlgoImplName(const std::string &implName, const Config &config, Config::Profile &prof, std::vector<optional<Device>> &devicesInUse, const std::vector<DeviceId> &allIds);

    }
}