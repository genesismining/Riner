//
//

#pragma once

#include <src/util/Bytes.h>
#include <cmath>

namespace riner {

    /**
     * Approximately calcualtes the difficulty (the amount of tries it takes to get a random 32 byte number smaller than target) from the target number
     * @param target target as a little endian 32 byte number
     * @return approximate difficulty
     */
    double targetToDifficultyApprox(Bytes<32> targetBytes);

    /**
     * The opposite of targetToDifficultyApprox (not exact inverse! only an approximation)
     * @return approximate target value (little endian)
     */
    Bytes<32> difficultyToTargetApprox(double difficulty);

}
