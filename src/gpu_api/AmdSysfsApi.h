#pragma once

#include <src/gpu_api/GpuApi.h>
#include <fstream>

namespace miner {

    class AmdSysfsApi : public GpuApi {

    public:

        ~AmdSysfsApi() override;

        optional<int> getEngineClock() override;
        optional<int> getMemoryClock() override;
        optional<int> getVoltage() override;
        optional<int> getTemperature() override;
        //optional<int> getFanPercent() override;
        optional<int> getFanRpm() override;
        optional<int> getPower() override;

    private:
        AmdSysfsApi() = default;

        friend class GpuApi;
        static std::unique_ptr<GpuApi> tryMake(const DeviceId &id);

    protected:
        bool readOnly = false;
        int numSclkStates = 0;
        int numMclkStates = 0;

        std::string pciePath;
        std::string hwmonPath;
        std::fstream sclk;
        std::fstream mclk;
        std::fstream fanPwm;
        std::fstream powerCap;
        std::fstream voltageTable;

        std::ifstream fanRpm;
        std::ifstream temp;
        std::ifstream power;
        std::ifstream voltage;
        std::ifstream memoryFreq;
        std::ifstream engineFreq;
    };

}
