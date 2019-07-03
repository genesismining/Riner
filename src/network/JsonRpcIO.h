//
//

#pragma once
#include "JsonIO.h"
#include "JsonRpcMessage.h"

namespace miner { namespace jrpc {

    class JsonRpcIO : public IOTypeLayer<Message, JsonIO> {

        Message convertIncoming(nl::json) override;
        nl::json convertOutgoing(Message) override;
    public:
        using IOTypeLayer::IOTypeLayer;

        ~JsonRpcIO() override {stopIOThread();}
    };

}}