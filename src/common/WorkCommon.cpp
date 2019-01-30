//
//

#include "WorkCommon.h"
#include <src/util/Logging.h>
#include <src/util/StringUtils.h>

namespace miner {

    AlgoEnum algoEnumFromString(const std::string &str) {
        if (toLower(str) == "ethash") return kEthash;
        return kAlgoTypeCount;
    }

    std::string stringFromAlgoEnum(AlgoEnum e) {
        switch(e) {
            case kEthash: return "ethash";
            default: return "invalid algo type";
        }
    }

    ProtoEnum protoEnumFromString(const std::string &str) {
        if (toLower(str) == "stratum+tcp") return kStratumTcp;
        return kProtoCount;
    }

    std::string stringFromProtoEnum(ProtoEnum e) {
        switch(e) {
            case kStratumTcp: return "stratum+tcp";
            default: return "invalid protocol";
        }
    }


}