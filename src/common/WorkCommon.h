
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

    template<size_t N>
    using Bytes = std::array<uint8_t, N>;

}