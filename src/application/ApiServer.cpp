
#include "ApiServer.h"
#include <numeric>
#include <src/algorithm/Algorithm.h>
#include <src/common/Assert.h>
#include <src/common/Chrono.h>
#include <src/application/Application.h>
#include <src/pool/PoolSwitcher.h>
#include <src/statistics/PoolRecords.h>

namespace riner {
    using namespace jrpc;

    ApiServer::ApiServer(uint16_t port, const Application &app)
            : _app(app)
            , io(std::make_unique<JsonRpcUtil>("ApiServer")){
        registerFunctions();

        bool success = io->launchServer(port, [this] (CxnHandle cxn) {
            //on connected
            VLOG(0) << "ApiServer connection #" << cxn.id() << " accepted";
            io->setReadAsyncLoopEnabled(true);
            io->readAsync(cxn);
        }, [] {
            VLOG(0) << "ApiServer connection closed";
        });

        if (success) {
            LOG(INFO) << "tcp JsonRPC 2.0 monitoring api now available on port " << port;
        }
        else {
            //likely because the port is already used
            LOG(WARNING) << "tcp JsonRPC 2.0 monitoring api not running";
        }
    }

    ApiServer::~ApiServer() {
        // manually destroy jrpc first, so the registered functions don't access
        // destroyed members asynchronously by accident
        io.reset();
    }

    static nl::json jsonSerialize(const decltype(DeviceRecords::Data::scannedNonces) &avg, clock::time_point time) {
        return {
                {"totalHashes", avg.mean.getTotalWeight()},
                {"totalReports", avg.mean.getTotal()},
                {"totalHashesPerSec", avg.mean.getWeightRate(time)},
                {"5sHashesPerSec", avg.avg5s.getWeightRate(time)},
                {"5sReportsPerSec", avg.avg5s.getRate(time)},
                {"30sHashesPerSec", avg.avg30s.getWeightRate(time)},
                {"30sReportsPerSec", avg.avg30s.getRate(time)}
        };
    }

    static nl::json jsonSerialize(const decltype(DeviceRecords::Data::validWorkUnits) &avg, clock::time_point time) {
        return {
                {"totalCount", avg.mean.getTotal()},
                {"totalHashesPerSec", avg.mean.getWeightRate(time)},
                {"5mHashesPerSec", avg.avg5m.getWeightRate(time)},
                {"5mWorkUnitsPerSec", avg.avg5m.getRate(time)}
        };
    }

    static nl::json jsonSerialize(const decltype(PoolRecords::Data::acceptedShares) &avg, clock::time_point time) {
        return {
            {"totalCount", avg.mean.getTotal()},
            {"connectionCount", avg.interval.getTotal()},
            {"totalHashesPerSec", avg.mean.getWeightRate(time)},
            {"connectionHashesPerSec", avg.interval.getWeightRate(time)},
            {"5mHashesPerSec", avg.avg5m.getWeightRate(time)},
            {"5mSharesPerSec", avg.avg5m.getRate(time)}
        };
    }

    static nl::json jsonSerialize(const PoolRecords::Data &data) {
        clock::time_point time = clock::now();
        return {
            {"accepted", jsonSerialize(data.acceptedShares, time)},
            {"rejected", jsonSerialize(data.rejectedShares, time)},
            {"duplicate", jsonSerialize(data.duplicateShares, time)}
        };
    }

    void ApiServer::registerFunctions() {
        //json exceptions will get caught by io

        //example divide method
        io->addMethod("divide", [] (float a, float b) {

            if (b == 0)
                throw jrpc::Error{invalid_params, "division by zero"};

            return a / b;

        }, "a", "b");

        io->addMethod("getGpuStats", [&] () {

            nl::json result;
            auto now = clock::now();

            auto devicesInUseLocked = _app.devicesInUse.readLock();

            size_t i = 0;
            for (const optional<Device> &deviceInUse : *devicesInUseLocked) {

                nl::json j;

                if (deviceInUse) {
                    const Device &d = *deviceInUse;

                    RNR_EXPECTS(i == d.deviceIndex); //just to be sure

                    auto data = d.records.read();

                    j = {
                            {"index", d.deviceIndex},
                            {"isInUse", true},
                            {"algoImpl", d.settings.algoImplName},
                            {"deviceVendor", stringFromVendorEnum(d.id.getVendor())},
                            {"deviceName", d.id.getName()},
                            {"scannedNonces", jsonSerialize(data.scannedNonces, now)},
                            {"workUnits", jsonSerialize(data.validWorkUnits, now)},
                            {"hwErrors", jsonSerialize(data.invalidWorkUnits, now)}
                    };
                    if (d.api) {
                        nl::json hw = nl::json::object();
                        if (auto temp = d.api->getTemperature()) {
                            hw["temperature_C"] = *temp;
                        }
                        if (auto voltage = d.api->getVoltage()) {
                            hw["voltage_mV"] = *voltage;
                        }
                        if (auto freq = d.api->getEngineClock()) {
                            hw["coreFrequency_MHz"] = *freq;
                        }
                        if (auto freq = d.api->getMemoryClock()) {
                            hw["memoryFrequency_MHz"] = *freq;
                        }
                        if (auto percent = d.api->getFanPercent()) {
                            hw["fanPercent"] = *percent;
                        }
                        if (auto rpm = d.api->getFanRpm()) {
                            hw["fanRpm"] = *rpm;
                        }
                        if (auto power = d.api->getPower()) {
                            hw["power_W"] = *power;
                        }
                        j["hwReports"] = std::move(hw);
                    }
                } else {
                    j = {
                            {"index", i},
                            {"isInUse", false},
                    };
                }

                result.push_back(j);
                ++i;
            }

            return result;
        });

        io->addMethod("getPoolStats", [&] () {

            nl::json result;

            auto lockedPoolSwitchers = _app.poolSwitchers.readLock();
            for (auto &poolSwitcherPair : *lockedPoolSwitchers) {
                std::string powType = poolSwitcherPair.first;
                auto &poolSwitcher = poolSwitcherPair.second;

                RNR_EXPECTS(poolSwitcher->getPowType() == powType);

                nl::json switcheri;

                const auto &pools = poolSwitcher->getPoolsData();

                for (auto &pool : pools) {
                    const auto &data = pool->readRecords();
                    nl::json poolj = {
                            {"url", pool->constructionArgs.host + ":" + std::to_string(pool->constructionArgs.port)},
                            {"shares", jsonSerialize(data)},
                            {"connected", pool->isConnected()},
                            {"connectionSec", std::chrono::duration<double>(data.connectionDuration()).count()}
                    };

                    switcheri.push_back(poolj);
                }

                const auto &data = poolSwitcher->readRecords();
                result[powType] = {
                        {"pools", switcheri},
                        {"shares", jsonSerialize(data)}
                };
            }
            return result;
        });

    }

}
