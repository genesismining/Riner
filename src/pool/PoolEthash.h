
#pragma once

#include <src/pool/Pool.h>
#include <src/pool/WorkEthash.h>
#include <src/network/TcpJsonProtocolUtil.h>
#include <src/application/Config.h>
#include <src/util/LockUtils.h>
#include <src/pool/WorkQueue.h>
#include <src/common/Pointers.h>
#include <queue>
#include <future>
#include <list>
#include <atomic>
#include <asio/yield.hpp>

namespace miner {
    template<class T>
    class AutoRefillQueue;
    class TcpJsonSubscription;

    struct EthashStratumProtocolData : public WorkProtocolData {
        std::string jobId;
    };

    class PoolEthashStratum : public WorkProvider {
    public:
        explicit PoolEthashStratum(PoolConstructionArgs);
        ~PoolEthashStratum() override;

    private:
        PoolConstructionArgs args;

        // WorkProvider interface
        optional<unique_ptr<WorkBase>> tryGetWork() override;
        void submitWork(unique_ptr<WorkResultBase> result) override;

        using WorkQueueType = AutoRefillQueue<unique_ptr<Work<kEthash>>>;
        unique_ptr<WorkQueueType> workQueue;

        // Pool Uid
        uint64_t getPoolUid() const override;
        uint64_t uid = createNewPoolUid();

        std::atomic<bool> shutdown {false};

        std::vector<std::shared_ptr<EthashStratumProtocolData>> protocolDatas;

        std::list<int> pendingShareIds; //submitted shares that have not yet been accepted or rejected
        bool isPendingShare(int id) const;
        int highestJsonRpcId = 0;

        void tcpCoroutine(nl::json response, asio::error_code, asio::coroutine &);
        void resetCoroutine(asio::coroutine &coro);
        unique_ptr<TcpJsonProtocolUtil> tcp;

        bool acceptsMiningNotifyMessages = false;

        std::string makeMiningSubscribeMsg();
        std::string makeMiningAuthorizeMsg();
        std::string makeSubmitMsg(const WorkResult<kEthash> &,
                const EthashStratumProtocolData &, int shareid);

        void onMiningNotify (nl::json &j);
        void onSetExtraNonce(nl::json &j);
        void onSetDifficulty(nl::json &j);
        void onShareAcceptedOrDenied(nl::json &j);
    };

}