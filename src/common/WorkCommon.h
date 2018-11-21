
#pragma once

#include <inttypes.h>
#include <array>

namespace miner {

    enum AlgoEnum : uint8_t {
        kEthash,

        kAlgoTypeCount,
    };

    enum ProtoEnum : uint8_t {
        kStratum,

        kProtoCount,
    };

}