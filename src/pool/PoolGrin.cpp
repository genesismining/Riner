
#include "PoolGrin.h"

#include <src/util/Logging.h>
#include <src/util/HexString.h>
#include <src/common/Json.h>
#include <src/common/Endian.h>
#include <chrono>
#include <random>
#include <functional>
#include <asio.hpp>

namespace miner {

    void PoolGrinStratum::onConnected(CxnHandle cxn) {

        jrpc::Message login = jrpc::RequestBuilder{}
            .id(io.nextId++)
            .method("login")
            .param("agent", "grin-miner")
            .param("login", args_.username)
            .param("pass", args_.password)
            .done();

        io.callAsync(cxn, login, [this] (CxnHandle cxn, jrpc::Message response) {
            if (response.isError()) {
                std::this_thread::sleep_for(std::chrono::seconds(1));
                onConnected(cxn);
                return;
            }

            jrpc::Message getjobtemplate = jrpc::RequestBuilder{}
                .id(io.nextId++)
                .method("getjobtemplate")
                .done();

            io.callAsync(cxn, getjobtemplate, [this] (CxnHandle cxn, jrpc::Message response) {
                if (auto result = response.getIfResult()) {
                    _cxn = cxn;
                    onMiningNotify(result.value());
                }
            });

        });

        io.addMethod("job", [this] (nl::json params) {
            onMiningNotify(params);
        });

        io.setIncomingModifier([this] (jrpc::Message &) {
            onStillAlive();
        });
    }
//
//    void PoolGrinStratum::restart() {
//        auto login = std::make_shared<JrpcBuilder>();
//        auto getjobtemplate = std::make_shared<JrpcBuilder>();
//
//        login->method("login")
//            .param("agent", "grin-miner")
//            .param("login", args_.username)
//            .param("pass", args_.password);
//
//        login->onResponse([this, getjobtemplate] (const JrpcResponse& ret) {
//            if (ret.error()) {
//                restart();
//                return;
//            }
//            jrpc.call(*getjobtemplate);
//        });
//
//        getjobtemplate->method("getjobtemplate");
//        getjobtemplate->onResponse([this] (auto &ret) {
//            if (!ret.error()) {
//                onMiningNotify(ret.getJson().at("result"));
//            }
//        });
//
//        //set handler for receiving incoming messages that are not responses to sent rpcs
//        jrpc.setOnReceiveUnhandled([this] (const JrpcResponse& ret) {
//            if (ret.atEquals("method", "job")) {
//                onMiningNotify(ret.getJson().at("params"));
//            } else {
//                LOG(WARNING) << "Ignoring unexpected message from server: " << ret.getJson().dump();
//            }
//        });
//
//        //set handler that gets called on any received response, before the other handlers are called
//        jrpc.setOnReceive([this] (auto &ret) {
//            onStillAlive(); //set latest still-alive timestamp to now
//        });
//
//        jrpc.call(*login);
//    }

    void PoolGrinStratum::onMiningNotify(const nl::json &jparams) {
        int64_t height = jparams.at("height");
        bool cleanFlag = (currentHeight != height);
        currentHeight = height;

        auto jobId = jparams.at("job_id").get<int64_t>();
        auto job = std::make_unique<GrinStratumJob>(jobId, height);

        job->workTemplate.difficulty = jparams.at("difficulty");
        job->workTemplate.nonce = random_.getUniform<uint64_t>();

        HexString powHex(jparams.at("pre_pow"));
        job->workTemplate.prePow.resize(powHex.sizeBytes());
        powHex.getBytes(job->workTemplate.prePow);

        setMaster(std::move(job), cleanFlag);
    }

    void PoolGrinStratum::submitSolutionImpl(unique_ptr<WorkSolution> resultBase) {
        auto result = static_unique_ptr_cast<WorkSolutionCuckatoo31>(std::move(resultBase));

        //build and send submitMessage on the tcp thread

        io.postAsync([this, result = std::move(result)] {

            auto job = result->getCastedJob<GrinStratumJob>();
            if (!job) {
                LOG(INFO) << "work result cannot be submitted because it has expired";
                return; //work has expired
            }

            nl::json pow;
            for(uint32_t i: result->pow) {
                pow.push_back(i);
            }

            jrpc::Message submit = jrpc::RequestBuilder{}.id(io.nextId++)
                    .param("edge_bits", result->pow.size())
                    .param("height", job->height)
                    .param("job_id", job->jobId)
                    .param("nonce", result->nonce)
                    .param("pow", pow)
                    .done();

            auto onResponse = [] (CxnHandle cxn, jrpc::Message res) {
                auto idStr = res.id.empty() ? res.id.get<std::string>() : "<no id>";
                bool accepted = false;
                if (auto result = res.getIfResult()) {
                    if (result.value() == "ok")
                        accepted = true;
                }

                std::string acceptedStr = accepted ? "accepted" : "rejected";
                LOG(INFO) << "share with id " << idStr << " got " << acceptedStr;

                if (!accepted) {
                    if (auto error = res.getIfError()) {
                        auto code = error.value().code;
                        LOG_IF(code == -32502, FATAL) << "Invalid share";
                    }
                }
            };

            auto onNeverResponded = [submit] () {
                //this handler gets called if there was no response after the last try
                auto idStr = submit.id.empty() ? submit.id.get<std::string>() : "<no id>";

                LOG(INFO) << "share with id " << idStr << " got discarded after pool did not respond multiple times";
            };

            io.callAsyncRetryNTimes(_cxn, submit, 5, std::chrono::seconds(5), onResponse, onNeverResponded);
        });
    }

    PoolGrinStratum::PoolGrinStratum(PoolConstructionArgs args)
            : PoolWithoutWorkQueue()
            , args_(std::move(args))
            , io(IOMode::Tcp) {

        //jrpc on-restart handler gets called when the connection is first established, and whenever a reconnect happens
        //jrpc.setOnRestart([this] {
        //    restart();
        //});

        io.launchClientAutoReconnect(args_.host, args_.port, [this] (CxnHandle cxn) {
            onConnected(cxn);
            io.setReadAsyncLoopEnabled(true);
            io.readAsync(cxn);//start listening
        });

        io.io().layerBelow().setIncomingModifier([] (nl::json &json) {
            try {
                json["id"] = std::stoi(json.at("id").get<std::string>());
            } catch(std::invalid_argument&) {
                json["id"] = 0;
            }
        });

        JsonIO &jsonLayer = io.io().layerBelow(); //Json layer is below Jrpc layer
        jsonLayer.setOutgoingModifier([] (nl::json &json) { //plug in a modifier func to change its id to string right before any json gets send
            json["id"] = std::to_string(json.at("id").get<jrpc::JsonRpcUtil::IdType>());
        });
    }

    PoolGrinStratum::~PoolGrinStratum() {
        shutdown = true;
    }

    cstring_span PoolGrinStratum::getName() const {
        return args_.host;
    }
}
