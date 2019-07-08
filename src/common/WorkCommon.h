
#pragma once

#include <string>

namespace miner {

    // TODO: remove this header and WorkCommon.cpp

    enum ProtoEnum : uint8_t {
        kStratumTcp,

        kProtoCount,
    };

    //returns kProtoCount if no match is found
    ProtoEnum protoEnumFromString(const std::string &);
    std::string stringFromProtoEnum(ProtoEnum);

}
