
#pragma once

#include <src/pool/Pool.h>
#include <src/pool/Work.h>
#include <src/common/Pointers.h>

namespace miner {

    template<>
    class Work<kEthash> : public WorkBase {
    public:
        Work(std::weak_ptr<WorkProtocolData> data) : WorkBase(std::move(data)) {}

        uint32_t extraNonce;
        //uint32_t minerNonce; //gets chosen by

        Bytes<32> target; //previously difficulty
        Bytes<32> header;

        uint32_t epoch = 0;
        Bytes<32> seedHash;

        AlgoEnum getAlgoEnum() const override {
            return kEthash;
        }
    };

    template<>
    class WorkResult<kEthash> : public WorkResultBase {
    public:
        WorkResult(std::weak_ptr<WorkProtocolData> data) : WorkResultBase(std::move(data)) {}

        uint64_t nonce;

        Bytes<32> proofOfWorkHash;
        Bytes<32> mixHash; // intermediate hash to prevent DOS

        AlgoEnum getAlgoEnum() const override {
            return kEthash;
        }
    };

}







