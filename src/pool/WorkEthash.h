
#pragma once

#include <src/pool/Pool.h>
#include <src/pool/Work.h>
#include <src/common/Pointers.h>
#include <src/util/Bytes.h>
#include <src/common/Span.h>

namespace miner {

    struct AlgorithmEthash {
        static inline constexpr auto &getName() {
            return "ethash";
        }
    };

    class WorkEthash : public Work, public AlgorithmEthash {
    public:
        WorkEthash(std::weak_ptr<WorkProtocolData> data) :
                Work(std::move(data), getName()) {
        }

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
    };

    class WorkSolutionEthash : public WorkSolution, public AlgorithmEthash {
    public:
        WorkSolutionEthash(std::weak_ptr<WorkProtocolData> data) :
                WorkSolution(std::move(data), getName()) {
        }

        uint64_t nonce = 0;

        Bytes<32> proofOfWorkHash;
        Bytes<32> mixHash; // intermediate hash to prevent DOS
    };

}







