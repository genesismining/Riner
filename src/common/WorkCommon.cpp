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

    ProtoEnum protoEnumFromString(const std::string &str) {
        if (toLower(str) == "stratum+tcp") return kStratumTcp;
        return kProtoCount;
    }

}