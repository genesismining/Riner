//
//

#pragma once

#include "src/network/JsonRpcUtil.h"
#include <string>

namespace riner {

    //launches a pool server for powType "dummy" on localhost
    //so that the entire mining process can be seen in action
    //when testing around with the dummy powType.
    //(this exists only for tutorial purposes to see PoolDummy and AlgoDummy in action)
    class DummyTestPoolServer {
        optional<CxnHandle> _cxn;

        jrpc::JsonRpcUtil io{"mock pool server we're connecting to |"};

    public:
        DummyTestPoolServer(uint16_t port);
    };

}