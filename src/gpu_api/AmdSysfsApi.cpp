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
        if (file.is_open()) {
            file.clear();
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

    template<typename T = int>
    static optional<T> getIntFromFile(std::ifstream &file) {
        optional<T> value = nullopt;
        if (file.is_open()) {
            file.clear();
            file.seekg(0);
            T tmp;
            file >> tmp;
            if (!file.fail())
                value = tmp;
        }
        return value;
    }

    optional<int> AmdSysfsApi::getEngineClock() {
        optional<int> freqMhz;
        optional<uint32_t> freqHz = getIntFromFile<uint32_t>(engineFreq);
        if (freqHz)
            freqMhz = (freqHz.value() + 500000U) / 1000000U;
        else
            freqMhz = getCurrentDpmFreq(sclk);
        return freqMhz;
    }

    optional<int> AmdSysfsApi::getMemoryClock() {
        optional<int> freqMhz;
        optional<uint32_t> freqHz = getIntFromFile<uint32_t>(memoryFreq);
        if (freqHz)
            freqMhz = (freqHz.value() + 500000U) / 1000000U;
        else
            freqMhz = getCurrentDpmFreq(mclk);
        return freqMhz;
    }

    optional<int> AmdSysfsApi::getVoltage() {
        return getIntFromFile(voltage);
    }

    optional<int> AmdSysfsApi::getTemperature() {
        auto t = getIntFromFile(temp);
        if (t)
            t = (t.value() + 500) / 1000;
        return t;
    }

    optional<int> AmdSysfsApi::getFanRpm() {
        return getIntFromFile(fanRpm);
    }

    optional<int> AmdSysfsApi::getPower() {
        auto t = getIntFromFile(power);
        if (t)
            t = (t.value() + 500000) / 1000000;
        return t;
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
            char path[128]{0};
            const auto &pciId = optPciId.value();
            std::snprintf(path, sizeof(path),
                          "/sys/bus/pci/devices/%.4x:%.2x:%.2x.%.1x/",
                          pciId.segment, pciId.bus, pciId.device, pciId.function);
            api->pciePath = path;

            std::memcpy(path + api->pciePath.size(), "hwmon", 6);
            DIR *hwmonDir = opendir(path);
            struct dirent *hwmonDirent;
            while (hwmonDir != nullptr && (hwmonDirent = readdir(hwmonDir)) != nullptr) {
                if (hwmonDirent->d_type == DT_DIR && !strncmp("hwmon", hwmonDirent->d_name, 5)) {
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
            api->temp.open(api->hwmonPath + "temp1_input");
            api->power.open(api->hwmonPath + "power1_average");
            api->voltage.open(api->hwmonPath + "in0_input");
            api->engineFreq.open(api->hwmonPath + "freq1_input");
            api->memoryFreq.open(api->hwmonPath + "freq2_input");

            api->sclk.open(api->pciePath + "pp_dpm_sclk");
            if (!api->sclk.is_open()) {
                api->sclk.open(api->pciePath + "pp_dpm_sclk", std::fstream::in); // read only
                api->readOnly |= api->sclk.is_open();
            }
            api->mclk.open(api->pciePath + "pp_dpm_mclk");
            if (!api->mclk.is_open()) {
                api->mclk.open(api->pciePath + "pp_dpm_mclk", std::fstream::in); // read only
                api->readOnly |= api->mclk.is_open();
            }
            api->vddcTable.open(api->pciePath + "pp_od_clk_voltage");
            api->fanPwm.open(api->hwmonPath + "pwm1");
            if (!api->fanPwm.is_open()) {
                api->fanPwm.open(api->hwmonPath + "pwm1", std::fstream::in); // read only
                api->readOnly |= api->fanPwm.is_open();
            }
            api->powerCap.open(api->hwmonPath + "power1_cap");

            std::string line;
            if (api->vddcTable.is_open()) {
                enum Stage {
                    UNDEFINED,
                    SCLK,
                    MCLK,
                    RANGE
                } stage;
                while (std::getline(api->vddcTable, line)) {
                    int idx, freq, voltage;
                    int min, max;
                    std::vector<table_entry> *table = nullptr;
                    if (line == "OD_SCLK:")
                        stage = SCLK;
                    else if (line == "OD_MCLK:")
                        stage = MCLK;
                    else if (line == "OD_RANGE:")
                        stage = RANGE;
                    else {
                        switch (stage) {
                            case SCLK:
                                table = &api->sclkTable;
                                break;
                            case MCLK:
                                table = &api->mclkTable;
                                break;
                            case RANGE:
                                if (sscanf(line.c_str(), "SCLK: %dMHz %dMHz", &min, &max) == 2)
                                    api->sclkRange = std::make_pair(min, max);
                                else if (sscanf(line.c_str(), "MCLK: %dMHz %dMHz", &min, &max) == 2)
                                    api->mclkRange = std::make_pair(min, max);
                                else if (sscanf(line.c_str(), "VDDC: %dmV %dmV", &min, &max) == 2)
                                    api->vddcRange = std::make_pair(min, max);
                                else {
                                    LOG(WARNING) << "Error while parsing OD_RANGE in " << api->pciePath << "pp_od_clk_voltage";
                                    stage = UNDEFINED;
                                }
                                break;
                            default:
                                break;
                        }
                        if (table != nullptr) {
                            if (sscanf(line.c_str(), "%d: %dMHz %dmV", &idx, &freq, &voltage) == 3 &&
                                idx == table->size()) {
                                table->emplace_back(table_entry{freq, voltage});
                            } else {
                                LOG(WARNING) << "Error while parsing " << api->pciePath << "pp_od_clk_voltage";
                                stage = UNDEFINED;
                            }
                        }
                    }
                }
            }
            else {
                while (std::getline(api->sclk, line)) {
                    int idx, freq;
                    if (sscanf(line.c_str(), "%d: %dMHz", &idx, &freq) == 2 && idx == api->sclkTable.size()) {
                        api->sclkTable.emplace_back(table_entry{freq});
                    }
                    else {
                        LOG(WARNING) << "Error while parsing " << api->pciePath << "pp_dpm_sclk";
                        break;
                    }
                }
                while (std::getline(api->mclk, line)) {
                    int idx, freq;
                    if (sscanf(line.c_str(), "%d: %dMHz", &idx, &freq) == 2 && idx == api->mclkTable.size()) {
                        api->mclkTable.emplace_back(table_entry{freq});
                    }
                    else {
                        LOG(WARNING) << "Error while parsing " << api->pciePath << "pp_dpm_mclk";
                        break;
                    }
                }
            }

            LOG(INFO) << "device " << api->pciePath.data() << " has sysfs API";
            return api;
        }
#endif
        LOG(INFO) << "device has no sysfs API";
        return nullptr;
    }

}