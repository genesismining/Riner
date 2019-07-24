
#pragma once

#include <src/pool/Pool.h>
#include <src/pool/Work.h>
#include <src/common/Pointers.h>
#include <src/util/Bytes.h>
#include <src/common/Span.h>

namespace miner {

    struct HasPowTypeEthash {
        static inline constexpr auto &getPowType() {
            return "ethash";
        }
    };

    class WorkEthash : public Work, public HasPowTypeEthash {

        static const uint32_t uniqueNonce;

    public:

        WorkEthash(std::weak_ptr<WorkProtocolData> data) :
                Work(std::move(data), getPowType()) {
        }

        uint32_t extraNonce = uniqueNonce;

        Bytes<32> target; //previously difficulty
        Bytes<32> header;

        uint32_t epoch = std::numeric_limits<uint32_t>::max();
        Bytes<32> seedHash;

        void setEpoch() {
            if (epoch == std::numeric_limits<uint32_t>::max()) {//calculate epoch for master if it didn't happen yet
                uint32_t getEthEpoch(cByteSpan<32>);
                epoch = getEthEpoch(seedHash);
            }
        }
    };

    class WorkSolutionEthash : public WorkSolution, public HasPowTypeEthash {
    public:
        WorkSolutionEthash(std::weak_ptr<WorkProtocolData> data) :
                WorkSolution(std::move(data), getPowType()) {
        }

        uint64_t nonce = 0;

        Bytes<32> header;
        Bytes<32> mixHash; // intermediate hash to prevent DOS
    };

}







