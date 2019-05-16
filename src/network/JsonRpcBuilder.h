//
//

#include "JsonRpcMessage.h"

namespace miner { namespace jrpc {

    class RequestBuilder { //builder pattern util for making a jrpc request
        Message _msg;
        Request &msgRequest();

        bool paramsIsArray();
        bool hasNoParams();

    public:
        RequestBuilder();

        RequestBuilder &id(nl::json id);
        RequestBuilder &method(String methodName);
        RequestBuilder &param(const char *key, nl::json val);
        RequestBuilder &param(nl::json val);

        Message done(); //call this at the end to convert to jrpc::Message
    };

}}
