//
//

#pragma once

#include <string>
#include <functional>
#include <src/common/Assert.h>
#include <src/util/TemplateUtils.h>
#include "JsonRpcMessage.h"
#include "JsonRpcInvoke.h"

namespace miner { namespace jrpc {

    //message = wrapped function with a name
    class Method {
        std::string _name;
        WrappedFunc _wrappedFunc;
    public:

        const std::string &name() const {
            return _name;
        }

        //only matches method name, no parameter check for function overloading
        bool matches(const Message &request) const {
            MI_EXPECTS(request.isRequest());
            return _name == request.var.value<Request>({}).method;
        }

        Message invoke(const Message &request) const {
            MI_EXPECTS(request.isRequest());
            return _wrappedFunc(request);
        }

        Method(std::string name, WrappedFunc wrappedFunc)
        : _name(std::move(name)), _wrappedFunc(std::move(wrappedFunc)) {
        };

    };

    //iterate through a container of Methods and call the one that matches
    //if theres no match, returns response message with Error jrpc::method_not_found
    template<class Cont>
    Message invokeMatchingMethod(const Cont &container, const Message &request) {
        //initialize with method_not_found
        Message response = Message{Response{Error{jrpc::method_not_found}}, request.id};

        for (const Method &method : container) {

            if (method.matches(request)) { //request["method"] == method.name
                response = method.invoke(request);
                break;
            }
        }

        return response;
    }

}}