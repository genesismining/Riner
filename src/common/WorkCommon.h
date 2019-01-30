
#pragma once

#include <inttypes.h>
#include <array>

namespace miner {

    enum AlgoEnum : uint8_t {
        kEthash,

        kAlgoTypeCount,
    };

    //returns kAlgoTypeCount if no match is found
    AlgoEnum algoEnumFromString(const std::string &);

    enum ProtoEnum : uint8_t {
        kStratumTcp,

        kProtoCount,
    };

    //returns kProtoCount if no match is found
    ProtoEnum protoEnumFromString(const std::string &);

}