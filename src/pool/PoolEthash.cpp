
#include "PoolEthash.h"
#include <src/pool/WorkEthash.h>
#include <src/util/Logging.h>
#include <chrono>
#include <src/network/TcpJsonSubscription.h>
#include <src/common/Json.h>
#include <src/util/HexString.h>
#include <src/pool/AutoRefillQueue.h>
#include <random>
#include <functional>
#include <algorithm> //max

#include <asio.hpp>

namespace miner {

    void PoolEthashStratum::resetCoroutine(asio::coroutine &coro) {
        //FIXME: acceptsMiningNotifyMessages state belongs to the coroutine, ideally this pool would define its own Coroutine subclass that stores state like that
        coro = asio::coroutine();
        acceptsMiningNotifyMessages = false;
        LOG(INFO) << "restarted connection coroutine";
        tcpCoroutine(nl::json(), asio::error_code(), coro);
    }

    #include <asio/yield.hpp>
    void PoolEthashStratum::tcpCoroutine(nl::json response, asio::error_code error, asio::coroutine &coro) {

        if (error || shutdown) {

            if (error) {
                LOG(INFO) << "tcpCoroutine: error: " << error.message();
            }
            else {
                LOG(INFO) << "tcpCoroutine: shutdown";
            }

            resetCoroutine(coro);
            //listener.isStillAlive(false);
            return;
        }

        //listener.isStillAlive(true);

        bool isShareResponse = false;

        //find out whether the id corresponds to a pending mining.submit message
        if (response.count("id")) {
            auto id = response.at("id").get<int>();
            isShareResponse = isPendingShare(id);
        }

        if (!acceptsMiningNotifyMessages) {
            if (response.count("method") && response.at("method") == "mining.notify") {
                LOG(INFO) << "ignoring received mining.notify message, because no mining.authorize happened yet.";
                tcp->asyncRead();
                return; //ignore this message
            }
        }

        reenter(coro) while (true) {

            yield tcp->asyncWrite(makeMiningSubscribeMsg(), true);
            yield tcp->asyncRead();

            if (!response.count("result") || response.at("result") != true) {
                yield break;
            }

            yield tcp->asyncWrite(makeMiningAuthorizeMsg(), true);
            yield tcp->asyncRead(); //TODO: what if this is already mining.notify work?

            if (!response.count("result") || response.at("result") != true) {
                yield break;
            }

            while (true) {
                acceptsMiningNotifyMessages = true;

                yield tcp->asyncRead();

                if (response.count("method")) {
                    if (response.at("method") == "mining.notify")
                        onMiningNotify(response);
                    else if (response.at("method") == "mining.set_extranonce")
                        onSetExtraNonce(response);
                    else if (response.at("method") == "mining.set_difficulty")
                        onSetDifficulty(response);
                    else {
                        yield break;
                    }
                }
                else if (isShareResponse) {
                    onShareAcceptedOrDenied(response);
                }
                else if (response.empty()) {
                    //this probably is the {} from an async_write, do nothing
                }
                else {
                    LOG(INFO) << "unexpected response in " << response;
                    yield break;
                }
            }
        }

        if (coro.is_complete())
            resetCoroutine(coro); //restart
    }
    #include <asio/unyield.hpp>

    std::string PoolEthashStratum::makeMiningSubscribeMsg() {
        char buffer[4096];

        auto *package = "sgminer";
        auto *version = "5.5.17-gm";

        auto *format = R"({"id": %d, "method": "mining.subscribe", "params": ["%s", "%s"]})" "\n";
        auto len = snprintf(buffer, sizeof(buffer), format,
                ++highestJsonRpcId, package, version);

        MI_EXPECTS(len >= 0 && len < sizeof(buffer));
        return std::string(buffer);
    }

    std::string PoolEthashStratum::makeMiningAuthorizeMsg() {
        char buffer[4096];

        auto *format = R"({"id": %d, "method": "mining.authorize", "params": ["%s", "%s"]})" "\n";
        auto len = snprintf(buffer, sizeof(buffer), format,
                ++highestJsonRpcId, args.username.c_str(), args.password.c_str());

        MI_EXPECTS(len >= 0 && len < sizeof(buffer));
        return std::string(buffer);
    }

    std::string PoolEthashStratum::makeSubmitMsg(const WorkResult<kEthash> &result,
            const EthashStratumProtocolData &protoData, int shareId) {

        auto hexNonce   = HexString(result.nonce).str();
        auto hexPowHash = HexString(result.proofOfWorkHash).str();
        auto hexMixHash = HexString(result.mixHash).str();

        char buffer[4096];
        auto *format = R"({"id": %d, "method": "mining.submit", "params": ["%s", "%s", "0x%s", "0x%s", "0x%s"]})" "\n";
        auto len = snprintf(buffer, sizeof(buffer), format,
                shareId, args.username.c_str(), protoData.jobId.c_str(), hexNonce.c_str(), hexPowHash.c_str(), hexMixHash.c_str());

        pendingShareIds.push_back(shareId);

        MI_EXPECTS(len >= 0 && len < sizeof(buffer));
        return std::string(buffer);
    }

    void PoolEthashStratum::onMiningNotify(nl::json &j) {
        auto jparams = j.at("params");

        bool cleanFlag = jparams.at(4);
        if (cleanFlag)
            protocolDatas.clear(); //invalidates all gpu work that links to them

        //create work package
        protocolDatas.emplace_back(std::make_shared<EthashStratumProtocolData>());
        auto &sharedProtoData = protocolDatas.back();
        auto weakProtoData = make_weak(sharedProtoData);

        auto work = std::make_unique<Work<kEthash>>(weakProtoData);

        sharedProtoData->jobId = jparams.at(0);
        HexString(jparams[1]).getBytes(work->header);
        HexString(jparams[2]).getBytes(work->seedHash);
        HexString(jparams[3]).flipByteOrder().getBytes(work->target);

        workQueue->setMaster(std::move(work), cleanFlag);
    }

    bool PoolEthashStratum::isPendingShare(int id) const {
        auto it = std::find(pendingShareIds.begin(), pendingShareIds.end(), id);
        return it != pendingShareIds.end();
    }

    void PoolEthashStratum::onSetExtraNonce(nl::json &j) {
        LOG(INFO) << "set extranonce not implemented yet";
    }

    void PoolEthashStratum::onSetDifficulty(nl::json &j) {
        LOG(INFO) << "set difficulty not implemented yet";
    }

    void PoolEthashStratum::onShareAcceptedOrDenied(nl::json &j) {
        auto id = j["id"].get<int>();

        bool accepted = false;
        if (j.count("result"))
            accepted = j["result"].get<bool>();

        bool tracked = isPendingShare(id);
        pendingShareIds.remove(id);

        std::string acceptedStr = accepted ? "accepted" : "rejected";
        std::string trackedStr = tracked ? "" : "untracked ";

        LOG(INFO) << "share with " << trackedStr << "id " << id << " got " << acceptedStr;
    }

    PoolEthashStratum::PoolEthashStratum(PoolConstructionArgs args) : args(args) {
        //initialize workQueue
        auto refillThreshold = 8; //once the queue size goes below this, lambda gets called

        auto refillFunc = [] (auto &out, auto &workMaster, size_t currentSize) {
            for (auto i = currentSize; i < 16; ++i) {
                ++workMaster->extraNonce;
                auto newWork = std::make_unique<Work<kEthash>>(*workMaster);
                out.push_back(std::move(newWork));
            }
            LOG(INFO) << "workQueue got refilled from " << currentSize << " to " << currentSize + out.size() << " items";
        };

        workQueue = std::make_unique<WorkQueueType>(refillThreshold, refillFunc);

        //initialize tcp protocol utility
        auto tcpFunc = [this] (auto response, auto &error, auto &coro) {
            tcpCoroutine(response, error, coro);
        };

        tcp = std::make_unique<TcpJsonProtocolUtil>(args.host, args.port, tcpFunc);
        tcp->launch();
    }

    PoolEthashStratum::~PoolEthashStratum() {
        shutdown = true;
    }

    optional<unique_ptr<WorkBase>> PoolEthashStratum::tryGetWork() {
        if (auto work = workQueue->popWithTimeout(std::chrono::milliseconds(100)))
            return std::move(work.value()); //implicit unique_ptr upcast
        return nullopt; //timeout
    }

    void PoolEthashStratum::submitWork(unique_ptr<WorkResultBase> result) {

        //build and send submitMessage on the tcp thread
        tcp->postAsync([this, result = static_unique_ptr_cast<WorkResult<kEthash>>(std::move(result))] {

            //obtain protoData shared_ptr (fails if associated work has expired)
            if (auto protoData = result->tryGetProtocolDataAs<EthashStratumProtocolData>()) {

                auto id = ++highestJsonRpcId;
                auto msg = makeSubmitMsg(*result, *protoData.value(), id);
                int tries = 0;

                auto submitRetryFunc = [this, msg, id, tries] () mutable {
                    if (tries > 0)
                        LOG(INFO) << "retrying to submit share with id " << id << "(try #" << tries+1 << ")";

                    if (tries > 5 || !isPendingShare(id)) {
                        pendingShareIds.remove(id); //remove if it wasn't already removed
                        return true; //don't retry
                    }

                    tcp->asyncWrite(msg);
                    ++tries;
                    return false; //retry later
                };

                tcp->asyncRetryEvery(std::chrono::seconds(5), submitRetryFunc);
            }
        });
    }

    uint64_t PoolEthashStratum::getPoolUid() const {
        return uid;
    }


}
