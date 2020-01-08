#pragma once

#include <src/compute/DeviceId.h>
#include <src/config/Config.h>
#include <src/util/Copy.h>
#include <functional>
#include <list>
#include <memory>

namespace miner {

    class GpuApi {

        static auto &getApis() {
            static std::list<std::function<std::unique_ptr<GpuApi>(const CtorArgs &)>> apis;
            return apis;
        }

    public:
        struct CtorArgs {
            DeviceId id;
            GpuSettings settings;
        };

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
        virtual optional<int> getTdp();

        virtual bool setEngineClock(int freq);
        virtual bool setMemoryClock(int freq);
        virtual bool setVoltage(int voltage);
        virtual bool setFanPercent(int percent);
        virtual bool setTdp(int tdp);

        /**
         * factory to instantiate a derived GpuApi class. If no API is available, then nullptr is returned
         * internally DerivedClass::tryMake is called for each DerivedClass registered with
         * GpuApi::Registry<DerivedClass>() while DerivedClass::tryMake returns nullptr
         */
        static std::unique_ptr<GpuApi> tryCreate(const CtorArgs &args);

        template<typename D>
        struct Registry {
            explicit Registry(const std::string &name) noexcept {
                LOG(DEBUG) << "register GpuApi: " << name;
                getApis().push_back(&D::tryMake);
            }

            Registry() = delete;
            DELETE_COPY_AND_MOVE(Registry);
        };

    protected:
        GpuApi() = default; // only factory function of derived class shall instantiate the class

    };

}