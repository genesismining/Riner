//
//

#include "JsonIO.h"

namespace miner {

    nl::json JsonIO::convertIncoming(std::string line) {

        bool allowExceptions = false;

        auto j = nl::json::parse(line, nullptr, allowExceptions);

        if (j.is_discarded()) {
            j = {}; //return empty json
            LOG(ERROR) << "JsonIO: json got discarded: '" << line << "'";
        }

        return j;
    }

    std::string JsonIO::convertOutgoing(nl::json j) {
        return j.dump() + '\n';
    }

}