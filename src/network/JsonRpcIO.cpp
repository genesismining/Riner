//
//

#include "JsonRpcIO.h"

namespace miner { namespace jrpc {

        Message JsonRpcIO::convertIncoming(nl::json j) {
            try {
                return Message{std::move(j)};
            } catch(const nl::json::exception &e) {
                throw IOConversionError{e.what()};
            }
        }

        nl::json JsonRpcIO::convertOutgoing(Message msg) {
            return msg.toJson();
        }

    }}