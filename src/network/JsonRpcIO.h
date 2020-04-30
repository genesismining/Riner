//
//

#pragma once
#include "JsonIO.h"
#include "JsonRpcMessage.h"

namespace riner { namespace jrpc {

    /**
     * `IOTypeLayer` for jrpc::Message, see `IOTypeLayer` for more information.
     */
    class JsonRpcIO : public IOTypeLayer<Message, JsonIO> {

        Message convertIncoming(nl::json) override;
        nl::json convertOutgoing(Message) override;
    public:
        using IOTypeLayer::IOTypeLayer; //expose base ctors

        ~JsonRpcIO() override {stopIOThread();}
    };

}}