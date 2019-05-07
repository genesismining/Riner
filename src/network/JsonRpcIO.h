//
//

#pragma once
#include "JsonIO.h"
#include <src/common/Variant.h>
#include <src/common/Optional.h>

namespace miner { namespace relaxedJsonRpc {

        //typedefs for mapping more directly to the Json Rpc spec
        using Null = std::nullptr_t;
        using String = std::string;
        using Number = int64_t;

        class Request {
            String jsonrpc; //should to be "2.0"
            String method;
            nl::json params;
            nl::json id;
        };

        class Response {
            
        };

        class Message {
            variant<std::vector<Request>,
                    std::vector<Response>> vec;
        };

        //the pools we communicate with do not strictly follow the JsonRpc spec
        //this is an attempt at covering most cases of non-conformity
        class JsonRpcIO : public IOTypeLayer<Message, JsonIO> {

            Message convertIncoming(nl::json j) override;
            nl::json convertOutgoing(Message j) override;
        public:
            using IOTypeLayer::IOTypeLayer;
        };

}}