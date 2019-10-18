//
//

#include "DifficultyTarget.h"

namespace miner {

    double targetToDifficultyApprox(Bytes<32> targetBytes) {
        using limits = std::numeric_limits<double>;

        //trying to approximate (2^256 - 1) / targetBytes
        uint64_t target;

        int offset = targetBytes.size() - 1; //31
        for (; offset >= sizeof(target) && targetBytes[offset] == 0; offset--);

        memcpy(&target, targetBytes.data() + offset - (sizeof(target)-1), sizeof(target));
        target = le64toh(target);

        //following line is basically: jobDifficulty = (0xFFFF... / (double)target) * 2^exp
        auto diff = ldexp((1. - limits::epsilon() / limits::radix) / target, /*bits*/8 * (sizeof(target) + ((targetBytes.size() - 1) - offset)));
        return diff;
    }

    Bytes<32> difficultyToTargetApprox(double difficulty) {
        using limits = std::numeric_limits<double>;

        auto target = uint64_t(ceil(ldexp((1. - limits::epsilon() / limits::radix) / difficulty, 64)));
        const auto &targetBytes = toBytesWithLittleEndian(target);

        Bytes<32> result;
        result.fill(0xff);
        memcpy(result.data() + 24, targetBytes.data(), 8);
        return result;
    }

}