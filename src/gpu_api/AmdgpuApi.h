#pragma once

#include <src/gpu_api/GpuApi.h>
#include <climits>
#include <fstream>
#include <mutex>

namespace miner {

    class AmdgpuApi : public GpuApi {

    public:

        ~AmdgpuApi() override;

        opt::optional<int> getEngineClock() override;
        opt::optional<int> getMemoryClock() override;
        opt::optional<int> getVoltage() override;
        opt::optional<int> getTemperature() override;
        opt::optional<int> getFanPercent() override;
        opt::optional<int> getFanRpm() override;
        opt::optional<int> getPower() override;
        opt::optional<int> getTdp() override;

        bool setEngineClock(int freq) override;
        bool setMemoryClock(int freq) override;
        bool setVoltage(int voltage) override;
        bool setFanPercent(int percent) override;
        bool setTdp(int tdp) override;

    private:

        friend class GpuApi;
        /**
         * tries to instantiate AmdgpuApi and if the API is not available, then nullptr is returned
         */
        static std::unique_ptr<GpuApi> tryMake(const CtorArgs &id);

    protected:

        AmdgpuApi() = default;

        struct table_entry {
            int freq;
            opt::optional<int> vddc;
            opt::optional<int> measuredVddc;
        };

        struct frequency_settings {
            char type;
            std::vector<table_entry> table;
            std::pair<int, int> range {0, INT_MAX};
            opt::optional<int> freqTarget;
            std::fstream dpm;
            std::ifstream currentFreq;
        };

        static bool setPowerstateRange(frequency_settings &settings, int begin, int end = INT_MAX);
        bool applyPowerplaySettings(frequency_settings &settings, int *pMaxState = nullptr);
        bool setPowerProfile(const std::string &mode);
        bool setFanProfile(int profile);

        bool readOnly = false;

        frequency_settings sclk {'s'};
        frequency_settings mclk {'m'};

        opt::optional<int> vddcTarget;
        std::pair<int, int> vddcRange;

        std::once_flag manualFan;
        std::once_flag manualPowerProfile;
        opt::optional<int> tdp;

        std::string pciePath;
        std::string hwmonPath;
        std::fstream fanPwm;
        std::fstream powerCap;
        std::fstream vddcTable;

        std::ifstream fanRpm;
        std::ifstream temp;
        std::ifstream power;
        std::ifstream voltage;
    };

}
