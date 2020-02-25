//
//

#include "JsonIO.h"

namespace miner {

    nl::json JsonIO::convertIncoming(std::string line) {

        bool allowExceptions = false;

        auto j = nl::json::parse(line, nullptr, allowExceptions);

        if (j.is_discarded()) {
            throw IOConversionError(std::string("JsonIO invalid json: '") + line + "'");
        }

        return j;
    }

    std::string JsonIO::convertOutgoing(nl::json j) {
        return j.dump() + '\n';
    }

}