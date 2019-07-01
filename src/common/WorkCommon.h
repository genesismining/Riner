
#pragma once

#include <string>

namespace miner {

    enum AlgoEnum : uint8_t {
        kEthash,
        kCuckatoo31,

        kAlgoTypeCount,
    };

    enum ProtoEnum : uint8_t {
        kStratumTcp,

        kProtoCount,
    };

    //returns kProtoCount if no match is found
    ProtoEnum protoEnumFromString(const std::string &);
    std::string stringFromProtoEnum(ProtoEnum);

}
