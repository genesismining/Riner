
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
            .param("login", constructionArgs.username)
            .param("pass", constructionArgs.password)
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
                    onMiningNotify(*result);
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


    void PoolGrinStratum::onMiningNotify(const nl::json &jparams) {
        int64_t height = jparams.at("height");
        bool cleanFlag = (currentHeight != height);
        currentHeight = height;

        auto jobId = jparams.at("job_id").get<int64_t>();
        auto job = std::make_unique<GrinStratumJob>(_this, jobId, height);

        job->workTemplate.difficulty = jparams.at("difficulty");
        job->workTemplate.nonce = random_.getUniform<uint64_t>();

        HexString powHex(jparams.at("pre_pow"));
        job->workTemplate.prePow.resize(powHex.sizeBytes());
        powHex.getBytes(job->workTemplate.prePow);

        queue.pushJob(std::move(job), cleanFlag);
    }

    bool PoolGrinStratum::isExpiredJob(const PoolJob &job) {
        return queue.isExpiredJob(job);
    }

    unique_ptr<Work> PoolGrinStratum::tryGetWorkImpl() {
        return queue.tryGetWork();
    }

    void PoolGrinStratum::submitSolutionImpl(unique_ptr<WorkSolution> solutionBase) {
        //build and send submitMessage on the tcp thread

        io.postAsync([this, solution = static_unique_ptr_cast<WorkSolutionCuckatoo31>(std::move(solutionBase))] {

            auto job = solution->tryGetJobAs<GrinStratumJob>();
            if (!job) {
                LOG(INFO) << "work result cannot be submitted because it has expired";
                return; //work has expired
            }

            nl::json pow;
            for(uint32_t i: solution->pow) {
                pow.push_back(i);
            }

            jrpc::Message submit = jrpc::RequestBuilder{}.id(io.nextId++)
                    .method("submit")
                    .param("edge_bits", solution->edgeBits())
                    .param("height", job->height)
                    .param("job_id", job->jobId)
                    .param("nonce", solution->nonce)
                    .param("pow", pow)
                    .done();

            auto onResponse = [this] (CxnHandle cxn, jrpc::Message res) {
                std::string idStr = "<no id>";
                if (!res.id.is_null()) {
                    idStr = std::to_string(res.id.get<int64_t>());
                }
                bool accepted = false;
                if (auto result = res.getIfResult()) {
                    if (*result == "ok") {
                        accepted = true;
                    }
                }

                // TODO: check whether difficulty of submitted share is 1
                records.reportShare(1., accepted, false);
                std::string acceptedStr = accepted ? "accepted" : "rejected";
                LOG(INFO) << "share with id " << idStr << " got " << acceptedStr;

                if (!accepted) {
                    if (auto error = res.getIfError()) {
                        auto code = error->code;
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

    void PoolGrinStratum::tryConnect() {
        auto &args = constructionArgs;

        JsonIO &jsonLayer = io.io().layerBelow(); //Json layer is below Jrpc layer

        jsonLayer.setIncomingModifier([] (nl::json &json) {
            auto methodIt = json.find("method");
            if (methodIt != json.end() && *methodIt == "job") {
                // actually this is a notification and notifications should have no id
                json.erase("id");
            }
            else {
                try {
                    auto idIt = json.find("id");
                    if (idIt != json.end())
                        *idIt = std::stoll(idIt->get<std::string>());
                } catch (std::invalid_argument &) {
                    // something strange happened
                    json["id"] = nullptr;
                }
            }
        });

        jsonLayer.setOutgoingModifier([] (nl::json &json) { //plug in a modifier func to change its id to string right before any json gets send
            json["id"] = std::to_string(json.at("id").get<jrpc::JsonRpcUtil::IdType>());
        });

        io.launchClientAutoReconnect(args.host, args.port, [this] (CxnHandle cxn) {
            onConnected(cxn);
            io.setReadAsyncLoopEnabled(true);
            io.readAsync(cxn);//start listening
        });
    }

    PoolGrinStratum::PoolGrinStratum(const PoolConstructionArgs &args)
            : Pool(args)
            , io(IOMode::Tcp) {
        tryConnect();
    }

    PoolGrinStratum::~PoolGrinStratum() {
    }

    void PoolGrinStratum::onDeclaredDead() {
        io.disconnectAll();
        tryConnect();
    }

}
