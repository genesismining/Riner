//
//

#include "JsonRpcHandlerMap.h"

namespace miner { namespace jrpc {

    void responseHandlerNoop(CxnHandle, const Message &) {}

    void HandlerMap::addForId(nl::json id, ResponseHandler &&handler) {
        _mmap.lock()->insert({std::move(id), make_unique<ResponseHandler>(std::move(handler))});
    }

    optional<ResponseHandler> HandlerMap::tryPopForId(const nl::json &id) {
        unique_ptr<ResponseHandler> handler;

        { auto mmap = _mmap.lock();

            auto it = mmap->find(id); //find first
            if (it != mmap->end()) {
                handler = std::move(it->second);
                mmap->erase(it); //erase one of the entries with this id
            }
        } //unlock

        if (handler)
            return std::move(*handler); //move handler out of unique_ptr
        else
            return nullopt;
    }

}}