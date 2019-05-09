#include "AmdgpuApi.h"
#include <climits>
#include <cstdio>
#ifdef __linux__
#include <sys/types.h>
#include <dirent.h>
#endif


namespace miner {

    static GpuApi::Registry<AmdgpuApi> registry {};

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

    template<typename T = int, typename Stream = std::ifstream>
    static optional<T> getIntFromFile(Stream &file) {
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

    static bool writeToFile(std::fstream &file, const std::string &str) {
        bool success = file.is_open();
        if (success) {
            file.clear();
            file.seekp(0);
            file << str << std::endl;
            success = file.good();
        }
        return success;
    }

     bool AmdgpuApi::setPowerstateRange(AmdgpuApi::frequency_settings &settings, int begin, int end) {
        bool success = true;
        std::stringstream ss;
        end = std::min(end, (int) settings.table.size());
        for (int state = begin; state < end; state++)
            ss << state << " ";
        std::string output = std::move(ss.str());
        if (!output.empty()) {
            output.resize(output.size() - 1);
            success = writeToFile(settings.dpm, output);
        }
        return success;
    }

    bool AmdgpuApi::applyPowerplaySettings(AmdgpuApi::frequency_settings &settings, bool commit) {
        bool isSet = !settings.freqTarget.has_value();
        int maxState = settings.table.size();
        if (readOnly || maxState == 0)
            return false;
        bool success = true;
        for (int state = 0; state < maxState; state++) {
            std::stringstream ss;
            const auto &entry = settings.table[state];
            int newFreq = entry.freq;
            if (!isSet && (newFreq >= settings.freqTarget.value() || state == maxState - 1)) {
                isSet = true;
                maxState = state + 1;
                newFreq = settings.freqTarget.value();
            }
            ss << settings.type << ' ' << state << " " << newFreq;
            if (entry.vddc.has_value()) {
                int voltage = std::min(entry.vddc.value(), vddcTarget.value_or(INT_MAX));
                if (entry.measuredVddc.has_value() && entry.measuredVddc.value() < voltage)
                    voltage = entry.vddc.value();
                ss << " " << voltage;
            }
            success &= writeToFile(vddcTable, ss.str());
        }
        if (commit)
            success &= writeToFile(vddcTable, "c");
        setPowerstateRange(settings, 0, maxState);
        return success;
    }

    bool AmdgpuApi::setPowerProfile(const std::string &mode) {
        std::fstream file(pciePath + "power_dpm_force_performance_level");
        return writeToFile(file, mode);
    }

    bool AmdgpuApi::setFanProfile(int profile) {
        std::fstream file(hwmonPath + "pwm1_enable");
        return writeToFile(file, std::to_string(profile));
    }

    optional<int> AmdgpuApi::getEngineClock() {
        optional<int> freqMhz;
        optional<uint32_t> freqHz = getIntFromFile<uint32_t>(sclk.currentFreq);
        if (freqHz)
            freqMhz = (freqHz.value() + 500000U) / 1000000U;
        else
            freqMhz = getCurrentDpmFreq(sclk.dpm);
        return freqMhz;
    }

    optional<int> AmdgpuApi::getMemoryClock() {
        optional<int> freqMhz;
        optional<uint32_t> freqHz = getIntFromFile<uint32_t>(mclk.currentFreq);
        if (freqHz)
            freqMhz = (freqHz.value() + 500000U) / 1000000U;
        else
            freqMhz = getCurrentDpmFreq(mclk.dpm);
        return freqMhz;
    }

    optional<int> AmdgpuApi::getVoltage() {
        return getIntFromFile(voltage);
    }

    optional<int> AmdgpuApi::getTemperature() {
        auto t = getIntFromFile(temp);
        if (t)
            t = (t.value() + 500) / 1000;
        return t;
    }

    optional<int> AmdgpuApi::getFanPercent() {
        auto pwm = getIntFromFile(fanPwm);
        if (pwm)
            pwm = (pwm.value() + 127) / 255;
        return pwm;
    }

    optional<int> AmdgpuApi::getFanRpm() {
        return getIntFromFile(fanRpm);
    }

    optional<int> AmdgpuApi::getPower() {
        auto p = getIntFromFile(power);
        if (p)
            p = (p.value() + 500000) / 1000000;
        return p;
    }

    optional<int> AmdgpuApi::getTdp() {
        auto tdp = getIntFromFile(powerCap);
        if (tdp)
            tdp = (tdp.value() + 500000) / 1000000;
        return tdp;
    }

    bool AmdgpuApi::setEngineClock(int freq) {
        std::call_once(manualPowerProfile, &AmdgpuApi::setPowerProfile, this, "manual");
        freq = std::min(std::max(sclk.range.first, freq), sclk.range.second);
        if (sclk.freqTarget.has_value() && sclk.freqTarget.value() == freq)
            return true;
        optional<int> oldFreq = sclk.freqTarget;
        setPowerstateRange(sclk, 0, 1);
        sclk.freqTarget = freq;
        bool success = applyPowerplaySettings(sclk);
        if (!success)
            sclk.freqTarget = oldFreq;
        return success;
    }

    bool AmdgpuApi::setMemoryClock(int freq) {
        std::call_once(manualPowerProfile, &AmdgpuApi::setPowerProfile, this, "manual");
        freq = std::min(std::max(mclk.range.first, freq), mclk.range.second);
        if (mclk.freqTarget.has_value() && mclk.freqTarget.value() == freq)
            return true;
        optional<int> oldFreq = mclk.freqTarget;
        mclk.freqTarget = freq;
        bool success = applyPowerplaySettings(mclk);
        if (!success)
            mclk.freqTarget = oldFreq;
        return success;
    }

    bool AmdgpuApi::setVoltage(int voltage) {
        std::call_once(manualPowerProfile, &AmdgpuApi::setPowerProfile, this, "manual");
        voltage = std::min(std::max(vddcRange.first, voltage), vddcRange.second);
        if (vddcTarget.has_value() && vddcTarget.value() == voltage)
            return true;
        optional<int> oldVddc = vddcTarget;
        vddcTarget = voltage;
        setPowerstateRange(sclk, 0, 1);
        bool success = applyPowerplaySettings(mclk, false) && applyPowerplaySettings(sclk);
        if (!success)
            vddcTarget = oldVddc;
        return success;
    }

    bool AmdgpuApi::setFanPercent(int percent) {
        std::call_once(manualFan, &AmdgpuApi::setFanProfile, this, 1);
        return writeToFile(fanPwm, std::to_string((255 * percent + 50) / 100));
    }

    bool AmdgpuApi::setTdp(int tdp) {
        return writeToFile(powerCap, std::to_string(1000000 * tdp));
    }


    AmdgpuApi::~AmdgpuApi() {
        if (pciePath.empty())
            return;
        setFanProfile(2); // switch to automatic fan
        if (tdp)
            setTdp(tdp.value());
        writeToFile(vddcTable, "r");
        writeToFile(vddcTable, "c"); // reset PowerplayTable
        setPowerProfile("auto");
        setPowerstateRange(sclk, 0);
        setPowerstateRange(mclk, 0);
    }

    std::unique_ptr<GpuApi> AmdgpuApi::tryMake(const CtorArgs &args) {
#ifdef __linux__
        auto api = std::unique_ptr<AmdgpuApi>(new AmdgpuApi());
        if (auto optPciId = args.id.getIfPcieIndex()) {
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
            api->sclk.currentFreq.open(api->hwmonPath + "freq1_input");
            api->mclk.currentFreq.open(api->hwmonPath + "freq2_input");

            api->sclk.dpm.open(api->pciePath + "pp_dpm_sclk");
            if (!api->sclk.dpm.is_open()) {
                api->sclk.dpm.open(api->pciePath + "pp_dpm_sclk", std::fstream::in); // read only
                api->readOnly |= api->sclk.dpm.is_open();
            }
            api->mclk.dpm.open(api->pciePath + "pp_dpm_mclk");
            if (!api->mclk.dpm.is_open()) {
                api->mclk.dpm.open(api->pciePath + "pp_dpm_mclk", std::fstream::in); // read only
                api->readOnly |= api->mclk.dpm.is_open();
            }
            api->vddcTable.open(api->pciePath + "pp_od_clk_voltage");
            api->fanPwm.open(api->hwmonPath + "pwm1");
            if (!api->fanPwm.is_open()) {
                api->fanPwm.open(api->hwmonPath + "pwm1", std::fstream::in); // read only
                api->readOnly |= api->fanPwm.is_open();
            }
            api->powerCap.open(api->hwmonPath + "power1_cap");
            if (!api->powerCap.is_open()) {
                api->powerCap.open(api->hwmonPath + "power1_cap", std::fstream::in); // read only
                api->readOnly |= api->powerCap.is_open();
            }

            std::string line;
            if (api->vddcTable.is_open()) {
                api->setPowerProfile("manual");
                enum Stage {
                    UNDEFINED,
                    SCLK,
                    MCLK,
                    RANGE
                } stage;
                while (std::getline(api->vddcTable, line)) {
                    int idx, freq, voltage;
                    int min, max;
                    optional<int> realVoltage;
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
                                table = &api->sclk.table;
                                setPowerstateRange(api->sclk, table->size(), table->size() + 1);
                                realVoltage = api->getVoltage();
                                if (realVoltage)
                                    realVoltage = realVoltage.value() + realVoltage.value() / 50; // add ~2 percent
                                break;
                            case MCLK:
                                table = &api->mclk.table;
                                break;
                            case RANGE:
                                if (sscanf(line.c_str(), "SCLK: %dMHz %dMHz", &min, &max) == 2)
                                    api->sclk.range = std::make_pair(min, max);
                                else if (sscanf(line.c_str(), "MCLK: %dMHz %dMHz", &min, &max) == 2)
                                    api->mclk.range = std::make_pair(min, max);
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
                                table->emplace_back(table_entry{freq, voltage, realVoltage});
                                LOG(INFO) << idx << ": " << voltage << "mV / " << realVoltage.value_or(-1) << "mV";
                            } else {
                                LOG(WARNING) << "Error while parsing " << api->pciePath << "pp_od_clk_voltage";
                                stage = UNDEFINED;
                            }
                        }
                    }
                }
                api->setPowerProfile("auto");
                setPowerstateRange(api->sclk, 0);
            }
            else {
                while (std::getline(api->sclk.dpm, line)) {
                    int idx, freq;
                    if (sscanf(line.c_str(), "%d: %dMHz", &idx, &freq) == 2 && idx == api->sclk.table.size()) {
                        api->sclk.table.emplace_back(table_entry{freq});
                    }
                    else {
                        LOG(WARNING) << "Error while parsing " << api->pciePath << "pp_dpm_sclk";
                        break;
                    }
                }
                while (std::getline(api->mclk.dpm, line)) {
                    int idx, freq;
                    if (sscanf(line.c_str(), "%d: %dMHz", &idx, &freq) == 2 && idx == api->mclk.table.size()) {
                        api->mclk.table.emplace_back(table_entry{freq});
                    }
                    else {
                        LOG(WARNING) << "Error while parsing " << api->pciePath << "pp_dpm_mclk";
                        break;
                    }
                }
            }
            api->tdp = api->getTdp();

            LOG(INFO) << "device " << api->pciePath.data() << " has sysfs API";
            return api;
        }
#endif
        LOG(INFO) << "device has no sysfs API";
        return nullptr;
    }

}