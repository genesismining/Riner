//
//

#pragma once

#include <src/util/Bytes.h>
#include <cmath>

namespace riner {

    double targetToDifficultyApprox(Bytes<32> targetBytes);

    Bytes<32> difficultyToTargetApprox(double difficulty);

}