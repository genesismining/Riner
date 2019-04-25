#include "AmdSysfsApi.h"
#include <climits>
#include <cstdio>
#ifdef __linux__
#include <sys/types.h>
#include <dirent.h>
#endif


namespace miner {

    static GpuApi::Registry<AmdSysfsApi> registry {};

    static optional<int> getCurrentDpmFreq(std::fstream &file) {
        optional<int> freq = nullopt;
        if (file.good()) {
            file.seekg(0);
            std::string table(std::istreambuf_iterator<char>(file), {});
            auto end_pos = table.find('*');
            if (end_pos != std::string::npos) {
                auto start_pos = table.rfind('\n', end_pos);
                if (start_pos == std::string::npos)
                    start_pos = 0;
                else
                    start_pos++;
                int tmp;
                if (sscanf(table.data() + start_pos, "%*d: %dMHz", &tmp) == 1)
                    freq = tmp;
            }
        }
        return freq;
    }

    static optional<int> getIntFromFile(std::ifstream &file) {
        optional<int> value = nullopt;
        if (file.good()) {
            file.seekg(0);
            int tmp;
            file >> tmp;
            if (!file.fail())
                value = tmp;
        }
        return value;
    }

    optional<int> AmdSysfsApi::getEngineClock() {
        optional<int> freq = getIntFromFile(engineFreq);
        if (!freq)
            freq = getCurrentDpmFreq(sclk);
        return freq;
    }

    optional<int> AmdSysfsApi::getMemoryClock() {
        optional<int> freq = getIntFromFile(memoryFreq);
        if (!freq)
            freq = getCurrentDpmFreq(mclk);
        return freq;
    }

    optional<int> AmdSysfsApi::getVoltage() {
        return getIntFromFile(voltage);
    }

    optional<int> AmdSysfsApi::getTemperature() {
        return getIntFromFile(temp);
    }

    optional<int> AmdSysfsApi::getFanRpm() {
        return getIntFromFile(fanRpm);
    }

    AmdSysfsApi::~AmdSysfsApi() {
        if (pciePath[0] == '\0')
            return;
        // reset fan speed control
        // reset power states
        // reset power limit
        // reset performance level
        // reset sclk and mclk limits
    }

    std::unique_ptr<GpuApi> AmdSysfsApi::tryMake(const DeviceId &id) {
#ifdef __linux__
        auto api = std::unique_ptr<AmdSysfsApi>(new AmdSysfsApi());
        if (auto optPciId = id.getIfPcieIndex()) {
            char path[128] {0};
            const auto &pciId = optPciId.value();
            std::snprintf(path, sizeof(path),
                    "/sys/bus/pci/devices/%.4x:%.2x:%.2x.%.1x/",
                    pciId.segment, pciId.bus, pciId.device, pciId.function);
            api->pciePath = path;

            std::memcpy(path + api->pciePath.size(), "hwmon", 6);
            DIR *hwmonDir = opendir(path);
            struct dirent *hwmonDirent;
            while (hwmonDir != nullptr && (hwmonDirent = readdir(hwmonDir)) != nullptr) {
                if (hwmonDirent->d_type == DT_DIR && strcmp("hwmon", hwmonDirent->d_name) == 0) {
                    api->hwmonPath = api->pciePath + "hwmon/" + hwmonDirent->d_name + "/";
                    break;
                }
            }
            if (hwmonDir != nullptr)
                closedir(hwmonDir);
            if (api->hwmonPath.empty())
                return nullptr;
            std::ifstream nameFile(api->hwmonPath + "name");
            std::string name;
            if (nameFile.good())
                std::getline(nameFile, name);
            if (name != "amdgpu")
                return nullptr;
            api->fanRpm.open(api->hwmonPath + "fan1_input");
            api->temp.open(api->hwmonPath + "temp1+input");
            api->power.open(api->hwmonPath + "power1_average");
            api->voltage.open(api->hwmonPath + "in0_input");
            api->engineFreq.open(api->hwmonPath + "freq1_input");
            api->memoryFreq.open(api->hwmonPath + "freq2_input");

            api->sclk.open(api->pciePath + "pp_dpm_sclk");
            api->mclk.open(api->pciePath + "pp_dpm_mclk");
            api->voltageTable.open(api->pciePath + "pp_od_clk_voltage");
            api->fanPwm.open(api->hwmonPath + "pwm1");
            api->powerCap.open(api->hwmonPath + "power1_cap");

            std::string line;
            while (std::getline(api->sclk, line))
                api->numSclkStates++;
            while (std::getline(api->mclk, line))
                api->numMclkStates++;

            LOG(INFO) << "device " << api->pciePath.data() << " has sysfs API";
            return api;
        }
#endif
        LOG(INFO) << "device has no sysfs API";
        return nullptr;
    }

}