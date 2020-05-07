//
//

#pragma once

#include <string>
#include "IOTypeLayer.h"
#include "BaseIO.h"

namespace riner {

    /**
     * `IOTypeLayer` for std::string lines, see `IOTypeLayer` for more information.
     *
     * since BaseIO already runs on `std::string` lines, this layer trivially takes
     * the `std::string` from the layer below and passes it on to the next.
     * If BaseIO is ever required to be refactored to work on another representation,
     * this layer can still persist as a conversion to `std::string` lines for any
     * implementations that may use its `setIncomingModifyFunc` or `setOutgoingModifyFunc`
     * features.
     */
    class LineIO : public IOTypeLayer<std::string, BaseIO> {
        unsigned verbosity_level = 0;
    public:
        using IOTypeLayer::IOTypeLayer; //expose base ctors

        std::string convertIncoming(std::string line) override {
            VLOG(verbosity_level) << "                                   " << "<-- " << line;
            return line;
        };

        std::string convertOutgoing(std::string line) override {
            VLOG(verbosity_level) << "                                   " << "--> " << line;
            return line;
        };

        void setLoggingVerbosity(unsigned level) {
            checkNotLaunchedOrOnIOThread();
            verbosity_level = level;
        }

        ~LineIO() override {stopIOThread();}
    };

}