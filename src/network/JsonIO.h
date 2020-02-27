//
//

#pragma once
#include "IOTypeLayer.h"
#include "LineIO.h"
#include <src/common/Json.h>

namespace riner {

    /**
     * `IOTypeLayer` for nl::json, see `IOTypeLayer` for more information.
     */
    class JsonIO : public IOTypeLayer<nl::json, LineIO> {

        nl::json    convertIncoming(std::string line) override;
        std::string convertOutgoing(nl::json j)       override;
    public:
        using IOTypeLayer::IOTypeLayer; //expose base ctors

        ~JsonIO() override {stopIOThread();}
    };

}