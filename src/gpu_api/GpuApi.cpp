#include "GpuApi.h"


namespace miner {

    std::list<std::function<std::unique_ptr<GpuApi>(const DeviceId &)>> GpuApi::apis{};

    optional<int> GpuApi::getEngineClock() {
        return nullopt;
    }

    optional<int> GpuApi::getMemoryClock() {
        return nullopt;
    }
    optional<int> GpuApi::getVoltage() {
        return nullopt;
    }
    optional<int> GpuApi::getTemperature() {
        return nullopt;
    }
    optional<int> GpuApi::getFanPercent() {
        return nullopt;
    }
    optional<int> GpuApi::getFanRpm() {
        return nullopt;
    }

    bool GpuApi::setEngineClock(int freq) {
        return false;
    }

    bool GpuApi::setMemoryClock(int freq) {
        return false;
    }

    bool GpuApi::setVoltage(int voltage) {
        return false;
    }

    bool GpuApi::setFanPercent(int percent) {
        return false;
    }

    std::unique_ptr<GpuApi> GpuApi::tryCreate(const DeviceId &id) {
        for (auto api : apis) {
            LOG(INFO) << "try to create API instance";
            if (auto instance = api(id))
                return instance;
        }
        return std::unique_ptr<GpuApi>();
    }

}