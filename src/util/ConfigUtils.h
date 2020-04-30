
#pragma once

#include <src/config/Config.h>
#include <src/compute/DeviceId.h>
#include <src/application/Device.h>
#include <vector>
#include <deque>

namespace riner {

    namespace configUtils {

        /**
         * loads a textproto file (=protocol buffer text format), which is then parsed as a Config message as specified in "src/config/Config.proto". if parsing fails, nullopt is returned.
         * @param configPath file path to textproto file
         * @return a valid Conifg object or nullopt
         */
        optional<Config> loadConfig(const std::string &configPath);

        /**
         * given a `profile` (see "src/config/Config.proto") this function goes through all contained tasks and returns a list of all AlgoImpls that have to be launched once the profile gets started. `deviceIds` must be passed to decide which tasks are relevant.
         * @param prof the profile in which to search
         * @param deviceIds vector of deviceIds of devices that the profile is run with
         * @return vector of AlgoImpl names compatible with `Registry`
         */
        std::vector<std::string> getUniqueAlgoImplNamesForProfile(const proto::Config_Profile &prof, const std::vector<DeviceId> &deviceIds);
    
        /**
         * selects the pool names (see "src/config/Config.proto") inside `config` which belong to a given `powType`
         * @param config the protobuf config object
         * @param powType the desired powType of pools
         * @return a vector of Config_Pool references with desired `powType` that point into the provided `config` object
         */
        std::vector<std::reference_wrapper<const proto::Config_Pool>> getConfigPoolsForPowType(const proto::Config &config,
                                                                                         const std::string &powType);

        /**
         * selects the `Task` (see "src/config/Config.proto") from a profile `prof` that the device with index `deviceIndex` is be assigned to.
         * @param prof the profile containing the tasks
         * @param deviceIndex a device index from `ComputeModule`
         * @return a reference to the device's task or `nullopt` if no corresponding task was found
         */
        optional_cref<proto::Config_Profile_Task> getTaskForDevice(const proto::Config_Profile &prof, size_t deviceIndex);

        /**
         * prepares the `assignedDevices` argument of `AlgoConstructionArgs` based on the provided `config`. Among other things this function looks up the appropriate `AlgoSettings` for every device based on config profile (see "src/config/Config.proto")
         * @param implName AlgoImpl name as used in `Registry`
         * @param config protobuf config object
         * @param prof config profile
         * @param devicesInUse the used devices, passed from `Application`
         * @param allIds list of all DeviceIds as provided by `ComputeModule`
         * @return references to initialized `Device`s, ready to be put into `AlgoConstructionArgs`
         */
        std::vector<std::reference_wrapper<Device>> prepareAssignedDevicesForAlgoImplName(const std::string &implName, const Config &config, const proto::Config_Profile &prof, std::deque<optional<Device>> &devicesInUse, const std::vector<DeviceId> &allIds);

    }
}
