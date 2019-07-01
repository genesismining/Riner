
#include "ApiServer.h"
#include <numeric>
#include <src/algorithm/Algorithm.h>
#include <src/common/Assert.h>
#include <src/pool/PoolSwitcher.h>

namespace miner {

    ApiServer::ApiServer(uint16_t port, const LockGuarded<std::deque<optional<Device>>> &devicesInUse,
            const std::array<unique_ptr<PoolSwitcher>, kAlgoTypeCount> &poolSwitchers)
            : devicesInUse(devicesInUse)
            , poolSwitchers(poolSwitchers)
            , jrpc(std::make_unique<JrpcServer>(port)) {
        LOG(INFO) << "started api server on port " << port;
        registerFunctions();
        jrpc->launch();
    }

    void ApiServer::registerFunctions() {
        //json exceptions will get caught by the caller of these functions

        jrpc->registerFunc("divide", [] (float a, float b) -> JrpcReturn {

            if (b == 0)
                return {JrpcError::invalid_params, "division by zero"};

            return a / b;

        }, "a", "b");

        jrpc->registerFunc("getGpuStats", [&] () -> JrpcReturn {

            nl::json result;
            auto now = clock::now();

            auto devicesInUseLocked = devicesInUse.lock();

            size_t i = 0;
            for (const optional<Device> &deviceInUse : *devicesInUseLocked) {

                nl::json j;

                if (deviceInUse) {
                    const Device &d = deviceInUse.value();

                    MI_EXPECTS(i == d.deviceIndex); //just to be sure

                    j = {
                            {"index", d.deviceIndex},
                            {"runs algorithm", true},
                            {"algo impl", d.settings.algoImplName},
                            {"device vendor", stringFromVendorEnum(d.id.getVendor())},
                            {"device name", gsl::to_string(d.id.getName())}
                    };

                    auto data = d.records.read();
                    auto &tn = data.traversedNonces;
                    auto tnPair = tn.interval.getAndReset();

                    j["traversed nonces"] = {
                            {{"kind", "exp average"}, {"exp average", tn.avg30s.getWeightRate(now)}, {"interval", "seconds"}, {"seconds", tn.avg30s.getDecayExp().count()}},
                            {{"kind", "exp average"}, {"exp average", tn.avg5m .getWeightRate(now)}, {"interval", "seconds"}, {"seconds", tn.avg5m .getDecayExp().count()}},
                            {{"kind", "average"}, {"average", tn.mean.getWeightRate(now)}, {"interval", "total"}},
                            {{"kind", "amount" }, {"amount" , static_cast<long long>(tn.mean.getTotalWeight())}, {"interval", "total"}},
                            {{"kind", "average"}, {"average", tn.mean.getWeightRate(now)}, {"interval", "total"}},
                            {{"kind", "amount" }, {"amount" , tnPair.first }, {"interval", "since last call"}},
                            {{"kind", "average"}, {"average", tnPair.second}, {"interval", "since last call"}},
                    };

                    auto &fsv = data.failedShareVerifications;

                    j["failed share verifications"] = {
                            {{"kind", "exp average"}, {"exp average", fsv.avg30s.getWeightRate(now)}, {"interval", "seconds"}, {"seconds", fsv.avg30s.getDecayExp().count()}},
                            {{"kind", "exp average"}, {"exp average", fsv.avg5m .getWeightRate(now) }, {"interval", "seconds"}, {"seconds", fsv.avg5m.getDecayExp().count()}},
                            {{"kind", "amount"}, {"amount" , static_cast<long long>(fsv.mean.getTotalWeight())}, {"interval", "total"}},
                            {{"kind", "average"}, {"average", fsv.mean.getWeightRate(now)}, {"interval", "total"}},
                            {{"kind", "amount"}, {"amount" , tnPair.first}, {"interval", "since last call"}},
                            {{"kind", "average"}, {"average" , tnPair.second}, {"interval", "since last call"}},
                    };

                } else {
                    j = {
                            {"index", i},
                            {"does run algorithm", false},
                    };
                }

                result.push_back(j);
                ++i;
            }

            return result;
        });

        jrpc->registerFunc("getPoolStats", [&] () -> JrpcReturn {

            nl::json result;

            for (auto algoIdx = 0; algoIdx < AlgoEnum::kAlgoTypeCount; ++algoIdx) {
                std::string algoTypeStr = Algorithm::algoEnumToString(static_cast<AlgoEnum>(algoIdx));

                nl::json algoj;

                if (auto &poolSwitcher = poolSwitchers.at(algoIdx)) {

                    auto info = poolSwitcher->gatherApiInfo();

                    for (auto &poolInfo : info.pools) {

                        nl::json poolj = {
                                {"ip", poolInfo.host + ":" + std::to_string(poolInfo.port)},
                        };

                        algoj.push_back(poolj);
                    }
                }

                result[algoTypeStr] = algoj;
            }
            return result;
        });

    }

    ApiServer::~ApiServer() {
        // manually destroy jrpc first, so the registered functions don't access
        // destroyed members asynchronously by accident
        jrpc.reset();
    }

}