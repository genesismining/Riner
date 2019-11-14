
#pragma once

#include <src/pool/Pool.h>
#include <src/pool/Work.h>
#include <src/common/Pointers.h>
#include <src/util/Bytes.h>
#include <src/common/Span.h>
#include <src/algorithm/ethash/DagCache.h>
#include <src/util/DifficultyTarget.h>

namespace miner {

    struct HasPowTypeEthash {
        static inline constexpr auto &getPowType() {
            return "ethash";
        }
    };

    class WorkEthash : public Work, public HasPowTypeEthash {
    public:

        WorkEthash(uint32_t extraNonce)
                : Work(getPowType()), extraNonce(extraNonce) {
        }

        Bytes<32> header;
        Bytes<32> seedHash;
        Bytes<32> jobTarget; //actual target of the PoolJob
        Bytes<32> deviceTarget; //easier target than above for GPUs

        double jobDifficulty;
        double deviceDifficulty = 60e6; //60 Mh, roughly 2s on a GPU

        uint32_t epoch = std::numeric_limits<uint32_t>::max();
        uint32_t extraNonce;

        void setEpoch() {
            if (epoch == std::numeric_limits<uint32_t>::max()) {//calculate epoch for workTemplate if it didn't happen yet
                epoch = calculateEthEpoch(seedHash);
            }
        }

        void setDifficultiesAndTargets(const Bytes<32> &jobTarget) {
            jobDifficulty = targetToDifficultyApprox(jobTarget);
            deviceDifficulty = std::min(deviceDifficulty, jobDifficulty); //make sure deviceDiff is not harder than jobDiff
            deviceTarget = difficultyToTargetApprox(deviceDifficulty);
        }
    };

    class WorkSolutionEthash : public WorkSolution, public HasPowTypeEthash {
    public:
        using work_type = WorkEthash;

        WorkSolutionEthash(const WorkEthash &work)
                : WorkSolution(static_cast<const Work&>(work))
                , jobDifficulty(work.jobDifficulty) {
        }

        Bytes<32> header;
        Bytes<32> mixHash; // intermediate hash to prevent DOS

        uint64_t nonce = 0;
        double jobDifficulty;
    };

}







