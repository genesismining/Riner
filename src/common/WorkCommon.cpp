//
//

#include "WorkCommon.h"
#include <src/util/StringUtils.h>

namespace miner {

    ProtoEnum protoEnumFromString(const std::string &str) {
        if (toLower(str) == "stratum+tcp") {
            return kStratumTcp;
        }
        return kProtoCount;
    }

    std::string stringFromProtoEnum(ProtoEnum e) {
        switch(e) {
            case kStratumTcp: return "stratum+tcp";
            default: return "invalid protocol";
        }
    }

}
