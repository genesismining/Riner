//
//

#pragma once

#include <map>
#include <functional>
#include <src/util/LockUtils.h>
#include <src/common/JsonForward.h>
#include <src/network/BaseIO.h>
#include <src/network/JsonRpcMessage.h>

namespace miner { namespace jrpc {

    using ResponseHandler = std::function<void(CxnHandle, const Message &response)>;
    void responseHandlerNoop(CxnHandle, const Message &);

    //basically behaves like a LockGuarded<std::multimap<Id, HandlerFunction>>
    //used for storing ids of sent jrpcs and their associated handlers
    //so that the handler can be invoked as soon as a response with that same id
    //is incoming.
    //multiple jrpcs with the same id can be submitted, however the handlers of
    //those ids are then most likely not going to be called in the assumed order
    class HandlerMap {
        LockGuarded<std::multimap<nl::json, unique_ptr<ResponseHandler>>> _mmap;
    public:
        //pushes a handler for the given id onto the map
        //supports pushing the same id multiple times
        void addForId(nl::json requestId, ResponseHandler &&);

        //returns nullopt if no handler with given id is inside the map
        //otherwise returns one of the handlers of that id and removes it from the map
        opt::optional<ResponseHandler> tryPopForId(const nl::json &responseId);
    };

}}
