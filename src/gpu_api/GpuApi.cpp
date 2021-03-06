#include <src/application/Registry.h>
#include "GpuApi.h"
#include "AmdgpuApi.h"


namespace riner {

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
    optional<int> GpuApi::getPower() {
        return nullopt;
    }
    optional<int> GpuApi::getTdp() {
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

    bool GpuApi::setTdp(int tdp) {
        return false;
    }

    std::unique_ptr<GpuApi> GpuApi::tryCreate(const GpuApiConstructionArgs &args) {
        Registry registry;

        //try to create
        for (std::string &gpuApiName : registry.listGpuApis()) {
            VLOG(1) << "trying to create GpuApi instance: '" << gpuApiName << "'";

            if (auto instance = registry.tryMakeGpuApi(gpuApiName, args)) {

                const auto &settings = args.settings;
                if (settings.power_limit_W)
                    instance->setTdp(*settings.power_limit_W);
                if (settings.core_voltage_mV)
                    instance->setVoltage(*settings.core_voltage_mV);
                if (settings.memory_clock_MHz)
                    instance->setMemoryClock(*settings.memory_clock_MHz);
                if (settings.core_clock_MHz)
                    instance->setEngineClock(*settings.core_clock_MHz);

                LOG(INFO) << "successfully created GpuApi instance: '" << gpuApiName << "'";
                return instance;
            }
            else {
                VLOG(1) << "failed to create GpuApi instance: '" << gpuApiName << "'";
            }
        }
        LOG(INFO) << "could not initialize any GpuApi";
        return nullptr;
    }

}
