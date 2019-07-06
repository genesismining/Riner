
#pragma once

#include <src/pool/Pool.h>
#include <src/pool/Work.h>
#include <src/common/Pointers.h>
#include <src/util/Bytes.h>
#include <src/common/Span.h>

namespace miner {

    template<>
    class Work<kEthash> : public WorkBase {
    public:
        Work(std::weak_ptr<WorkProtocolData> data) : WorkBase(std::move(data)) {}

        uint32_t extraNonce = 0;
        //uint32_t minerNonce;

        Bytes<32> target; //previously difficulty
        Bytes<32> header;

        uint32_t epoch = std::numeric_limits<uint32_t>::max();
        Bytes<32> seedHash;

        void setEpoch() {
            if (epoch == std::numeric_limits<uint32_t>::max()) {//calculate epoch for master if it didn't happen yet
                uint32_t EthCalcEpochNumber(cByteSpan<32>);
                epoch = EthCalcEpochNumber(seedHash);
            }
        }

        AlgoEnum getAlgoEnum() const override {
            return kEthash;
        }
    };

    template<>
    class WorkResult<kEthash> : public WorkResultBase {
    public:
        WorkResult(std::weak_ptr<WorkProtocolData> data) : WorkResultBase(std::move(data)) {}

        uint64_t nonce = 0;

        Bytes<32> proofOfWorkHash;
        Bytes<32> mixHash; // intermediate hash to prevent DOS

        AlgoEnum getAlgoEnum() const override {
            return kEthash;
        }
    };

}







