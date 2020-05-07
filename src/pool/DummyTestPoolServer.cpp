//
//

#include "DummyTestPoolServer.h"
#include "src/common/Chrono.h"

namespace riner {
    using namespace std::chrono_literals;

    DummyTestPoolServer::DummyTestPoolServer(uint16_t port) {
        //this class pretends to be an external pool server for tutorial's sake, so we don't want it to show up in the logs => set verbosity to 9
        io.io().layerBelow().layerBelow().setLoggingVerbosity(9);

        auto onCxn = [this] (CxnHandle cxn) {
            if (_cxn) return;

            _cxn = cxn;

            io.addMethod("mining.subscribe", [] (nl::json params) -> nl::json {
                return true; //creates "result":true response
            });

            io.addMethod("mining.authorize", [this] (nl::json params) -> nl::json {

                io.io().retryAsyncEvery(15s, [this, n = 0] () mutable {
                    if (n++ > 0) {
                        auto notify = jrpc::RequestBuilder{}.method("mining.notify")
                                .param("0x0123")
                                .param("0x4567")
                                .param("0x89AB")
                                .param("0xCDEF")
                                .done();

                        io.callAsync(*_cxn, notify);
                    }
                    return false;
                }, []{});

                return true;
            });

            io.setReadAsyncLoopEnabled(true);
            io.readAsync(cxn);
        };

        auto onDc = [this] {_cxn = nullopt;};

        io.launchServer(port, onCxn, onDc);
    }

}