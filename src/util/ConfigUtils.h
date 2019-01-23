
#pragma once

#include <src/application/Config.h>
#include <src/compute/DeviceId.h>
#include <vector>

namespace miner {

    struct DeviceAlgoInfo {
        Config::DeviceProfile::AlgoSettings settings;
        DeviceId id;
    };

    namespace configUtils {

        Config loadConfig(cstring_span configPath);

        std::vector<std::string> getUniqueAlgoImplNamesForProfile(Config::Profile &prof, const std::vector<DeviceId> &deviceIds);

        std::vector<std::reference_wrapper<const Config::Pool>> getConfigPoolsForAlgoType(const Config &config, AlgoEnum algoType);

        Config::Profile::Mapping getMappingForDevice(Config::Profile &prof, size_t deviceIndex);

        std::vector<DeviceAlgoInfo> getAllDeviceAlgoInfosForAlgoImplName(const std::string &implName, const Config &config, Config::Profile &prof, const std::vector<DeviceId> &allDevIds);

    }
}