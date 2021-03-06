
#include "PoolEthash.h"
#include <src/pool/WorkEthash.h>
#include <src/util/Logging.h>
#include <src/util/HexString.h>
#include <src/common/Json.h>
#include <src/common/portable_endian.h>
#include <chrono>
#include <random>
#include <functional>

#include <asio.hpp>
#include <src/common/Chrono.h>

namespace riner {

    void PoolEthashStratum::onConnected(CxnHandle cxn) {
        acceptMiningNotify = false;

        jrpc::Message subscribe = jrpc::RequestBuilder{}
            .id(io.nextId++)
            .method("mining.subscribe")
            .param("sgminer")
            .param("5.5.17-gm")
            .done();

        io.callAsync(cxn, subscribe, [this] (CxnHandle cxn, jrpc::Message response) {
            //this handler gets invoked when a jrpc response with the same id as 'subscribe' is received

            //return if it's not a {"result": true} message
            if (!response.isResultTrue()) {
                LOG(INFO) << "Pool disabled because mining subscribe failed: " << response.str();
                setDisabled(true);
                return;
            }

            jrpc::Message authorize = jrpc::RequestBuilder{}
                .id(io.nextId++)
                .method("mining.authorize")
                .param(constructionArgs.username)
                .param(constructionArgs.password)
                .done();

            io.callAsync(cxn, authorize, [this] (CxnHandle cxn, jrpc::Message response) {
                if (response.isResultTrue()) {
                    acceptMiningNotify = true;
                    _cxn = cxn; //store connection for submit
                }
                else {
                    LOG(INFO) << "Pool disabled because authorization failed (" << response.str() << "). Please check whether the correct pool credentials are supplied";
                    setDisabled(true);
                }
            });
        });

        io.addMethod("mining.notify", [this] (nl::json params) {
            if (acceptMiningNotify) {
                if (params.is_array() && params.size() >= 4)
                    onMiningNotify(params);
                else
                    throw jrpc::Error{jrpc::invalid_params, "expected at least 4 params"};
            }
        });

        io.setIncomingModifier([&] (jrpc::Message &msg) {
            //called everytime a jrpc::Message is incoming, so that it can be modified
            onStillAlive(); //update still alive timer

            //incoming mining.notify should be a notification (which means id = {}) but some pools
            //send it with a regular id. The io object will treat it like a notification once we
            //remove the id.
            if (msg.hasMethodName("mining.notify"))
                msg.id = {};
        });

        io.setReadAsyncLoopEnabled(true);
        io.readAsync(cxn); //start listening for incoming responses and rpcs from this cxn
    }

    void PoolEthashStratum::onMiningNotify(const nl::json &jparams) {
        bool cleanFlag = jparams.at(4);
        Bytes<32> jobTarget;
        const auto &jobId = jparams.at(0).get<std::string>();
        auto job = std::make_unique<EthashStratumJob>(_this, jobId);

        job->workTemplate.extraNonce = static_cast<uint32_t>(std::random_device()()); //generate random number for extranonce
        HexString(jparams[1]).getBytes(job->workTemplate.header);
        HexString(jparams[2]).getBytes(job->workTemplate.seedHash);
        HexString(jparams[3]).swapByteOrder().getBytes(jobTarget);
        job->workTemplate.setDifficultiesAndTargets(jobTarget);

        //workTemplate->epoch is calculated in EthashStratumJob::makeWork()
        //so that not too much time is spent on this thread.

        setConnected(true);
        queue.pushJob(std::move(job), cleanFlag);
    }

    void PoolEthashStratum::expireJobs() {
        queue.expireJobs();
    }

    void PoolEthashStratum::clearJobs() {
        queue.clear();
    }

    bool PoolEthashStratum::isExpiredJob(const PoolJob &job) {
        return queue.isExpiredJob(job);
    }

    void PoolEthashStratum::onDeclaredDead() {
        io.disconnectAll();
        tryConnect();
    }

    unique_ptr<Work> PoolEthashStratum::tryGetWorkImpl() {
        return queue.popWithTimeout();
    }

    void PoolEthashStratum::submitSolutionImpl(unique_ptr<WorkSolution> resultBase) {

        auto result = static_unique_ptr_cast<WorkSolutionEthash>(std::move(resultBase));

        //build and send submitMessage on the tcp thread

        io.postAsync([this, result = std::move(result)] {

            auto job = result->tryGetJobAs<EthashStratumJob>();
            if (!job) {
                LOG(INFO) << "work result cannot be submitted because it is stale";
                return; //work is stale
            }

            uint32_t shareId = io.nextId++;

            jrpc::Message submit = jrpc::RequestBuilder{}
                .id(shareId)
                .method("mining.submit")
                .param(constructionArgs.username)
                .param(job->jobId)
                .param("0x" + HexString(toBytesWithBigEndian(result->nonce)).str()) //nonce must be big endian
                .param("0x" + HexString(result->header).str())
                .param("0x" + HexString(result->mixHash).str())
                .done();

            auto onResponse = [this, difficulty = result->jobDifficulty] (CxnHandle cxn, jrpc::Message response) {
                records.reportShare(difficulty, response.isResultTrue(), false);
                std::string acceptedStr = response.isResultTrue() ? "accepted" : "rejected";
                LOG(INFO) << "share with id '" << response.id << "' got " << acceptedStr << " by '" << getName() << "'";
            };

            //this handler gets called if there was no response after the last try
            auto onNeverResponded = [shareId] () {
                // TODO: Shall we add dedicated statistics for this?
                LOG(INFO) << "share with id " << shareId << " got discarded after pool did not respond multiple times";
            };

            io.callAsyncRetryNTimes(_cxn, submit, 5, seconds(5), onResponse, onNeverResponded);

        });
    }

    void PoolEthashStratum::tryConnect() {
        setConnected(false);
        if (isDisabled() || !isActive()) {
            return;
        }
        auto &args = constructionArgs;
        io.launchClientAutoReconnect(args.host, args.port, [this] (auto cxn) {
            onConnected(cxn);
        }, [this] () {
            setConnected(false);
        });
    }

    PoolEthashStratum::PoolEthashStratum(const PoolConstructionArgs &args)
            : Pool(args) {
        if (args.sslDesc.client) {
            io.io().enableSsl(args.sslDesc);
        }
        tryConnect();
    }

    PoolEthashStratum::~PoolEthashStratum() {
    }

}
