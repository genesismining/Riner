#pragma once

#include <functional>
#include <list>
#include <memory>
#include <src/compute/DeviceId.h>

namespace miner {

    class GpuApi {

    private:
        static std::list<std::function<std::unique_ptr<GpuApi>(const DeviceId &)>> apis;

    protected:
        GpuApi() = default;

    public:
        virtual ~GpuApi() = default;
        GpuApi(const GpuApi &) = delete;
        GpuApi &operator=(const GpuApi &) = delete;

        virtual optional<int> getEngineClock();
        virtual optional<int> getMemoryClock();
        virtual optional<int> getVoltage();
        virtual optional<int> getTemperature();
        virtual optional<int> getFanPercent();
        virtual optional<int> getFanRpm();
        virtual optional<int> getPower();

        virtual bool setEngineClock(int freq);
        virtual bool setMemoryClock(int freq);
        virtual bool setVoltage(int voltage);
        virtual bool setFanPercent(int percent);

        static std::unique_ptr<GpuApi> tryCreate(const DeviceId &id);

        template<typename D>
        struct Registry {
            explicit Registry() noexcept {
                GpuApi::apis.push_back(&D::tryMake);
            }

            Registry(const Registry &) = delete;
            Registry& operator=(const Registry&) = delete;
        };
    };

}