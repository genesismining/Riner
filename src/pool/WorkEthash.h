
#pragma once

#include <src/pool/Pool.h>
#include <src/pool/Work.h>
#include <src/common/Pointers.h>
#include <src/util/Bytes.h>
#include <src/common/Span.h>
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
                uint32_t getEthEpoch(cByteSpan<32>);
                epoch = getEthEpoch(seedHash);
            }
        }

        void setDifficultiesAndTargetsNew(const Bytes<32> &jobTarget) {
            jobDifficulty = targetToDifficultyApprox(jobTarget);
            deviceDifficulty = std::min(deviceDifficulty, jobDifficulty); //make sure deviceDiff is not harder than jobDiff
            deviceTarget = difficultyToTargetApprox(deviceDifficulty);
        }

        // TODO: export this to proper functions
        void setDifficultiesAndTargets(const Bytes<32> &jobTarget) {
            using limits = std::numeric_limits<double>;
            int offset = 31;
            for (; offset >= 8 && jobTarget[offset] == 0; offset--);
            uint64_t target;
            memcpy(&target, jobTarget.data() + offset - 7, 8);
            target = le64toh(target);
            jobDifficulty = ldexp((1. - limits::epsilon() / limits::radix) / target, 64 + 8 * (31 - offset));
            this->jobTarget = jobTarget;

            deviceDifficulty = std::min(deviceDifficulty, jobDifficulty);
            setDeviceTarget(deviceDifficulty);

            MI_EXPECTS(jobDifficulty == targetToDifficultyApprox(jobTarget));
            MI_EXPECTS(deviceTarget == difficultyToTargetApprox(deviceDifficulty));
        }

    private:

        void setDeviceTarget(double difficulty) {
            using limits = std::numeric_limits<double>;
            auto target = uint64_t(ceil(ldexp((1. - limits::epsilon() / limits::radix) / difficulty, 64)));
            const auto &targetBytes = toBytesWithLittleEndian(target);
            deviceTarget.fill(0xff); // just to be safe...
            memcpy(deviceTarget.data() + 24, targetBytes.data(), 8);
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







