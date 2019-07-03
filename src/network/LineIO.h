//
//

#pragma once

#include <string>
#include "IOTypeLayer.h"
#include "BaseIO.h"

namespace miner {

    //this layer just trivially takes the string and leaves it as it is
    //it is there so that there is no code duplication for strings for all the
    //functionality that is already covered by IOTypeLayer

    class LineIO : public IOTypeLayer<std::string, BaseIO> {
    public:
        using IOTypeLayer::IOTypeLayer;

        std::string convertIncoming(std::string line) override {
            LOG(INFO) << "                                   " << "<-- " << line;
            return line;
        };

        std::string convertOutgoing(std::string line) override {
            LOG(INFO) << "                                   " << "--> " << line;
            return line;
        };

        ~LineIO() override {stopIOThread();}
    };

}