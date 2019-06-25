//
//

#pragma once
#include "IOTypeLayer.h"
#include "LineIO.h"
#include <src/common/Json.h>

namespace miner {

    class JsonIO : public IOTypeLayer<nl::json, LineIO> {

        nl::json    convertIncoming(std::string line) override;
        std::string convertOutgoing(nl::json j)       override;
    public:
        using IOTypeLayer::IOTypeLayer;
    };

}