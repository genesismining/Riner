#pragma once

#include <src/gpu_api/GpuApi.h>
#include <climits>
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

        struct table_entry {
            int freq;
            optional<int> vddc;
        };

        std::vector<table_entry> sclkTable;
        std::vector<table_entry> mclkTable;

        std::pair<int, int> sclkRange {0, INT_MAX};
        std::pair<int, int> mclkRange {0, INT_MAX};
        std::pair<int, int> vddcRange {0, INT_MAX};

        std::string pciePath;
        std::string hwmonPath;
        std::fstream sclk;
        std::fstream mclk;
        std::fstream fanPwm;
        std::fstream powerCap;
        std::fstream vddcTable;

        std::ifstream fanRpm;
        std::ifstream temp;
        std::ifstream power;
        std::ifstream voltage;
        std::ifstream memoryFreq;
        std::ifstream engineFreq;
    };

}
