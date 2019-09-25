//
//

#include <src/common/Assert.h>
#include "JsonRpcBuilder.h"

namespace miner { namespace jrpc {

        RequestBuilder::RequestBuilder() {
            _msg.var = Request{}; //make _msg be a request
            MI_ENSURES(var::holds_alternative<Request>(_msg.var));
        }

        RequestBuilder &RequestBuilder::id(nl::json id) {
            _msg.id = std::move(id);
            return *this;
        }

        Request &RequestBuilder::msgRequest() {
            return var::get<Request>(_msg.var);
        }

        RequestBuilder &RequestBuilder::method(String name) {
            msgRequest().method = std::move(name);
            return *this;
        }

        RequestBuilder &RequestBuilder::param(nl::json val) {
            MI_EXPECTS(paramsIsArray() || hasNoParams());

            msgRequest().params.emplace_back(std::move(val));
            return *this;
        }

        RequestBuilder &RequestBuilder::param(const char *key, nl::json val) {
            MI_EXPECTS(!paramsIsArray() || hasNoParams());

            msgRequest().params[key] = std::move(val);
            return *this;
        }

        bool RequestBuilder::hasNoParams() {
            return msgRequest().params.empty();
        }

        bool RequestBuilder::paramsIsArray() {
            return !hasNoParams() && msgRequest().params.is_array();
        }

        Message RequestBuilder::done() {
            return std::move(_msg);
        }

    }}