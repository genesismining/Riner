//
//

#pragma once

#include "JsonRpcMessage.h"
#include <src/common/Assert.h>

namespace miner { namespace jrpc {

    namespace invoke_helpers {

        template<class T>
        T extractSingleArg(const nl::json &jArgs, const std::string &argName) {
            return jArgs.at(argName).get<T>();
        }

        //this method is used to match Func's callOperator, so that the args types can be extracted
        template<class Func, class Ret, class Cls, class ... Args, class ... ArgNames>
        auto makeJrpcCallableHelper(Func func, Ret(Cls::* funcCallOperator)(Args...) const, ArgNames &&... argNames) {

            static_assert(sizeof...(Args) == sizeof...(ArgNames), "amount of args of function doesn't match amount of argNames, provide a name string for every argument! Don't provide more strings than arguments!");

            //create a wrapper that converts the Ret(Args...) function to a JsonResponse(const nl::json &) function
            return [func = std::move(func), argNames...] (const jrpc::Message &request) -> jrpc::Message {

                if (!request.isRequest()) {
                    //function was called with a message that is not a request
                    return Message{Response{Error{jrpc::invalid_request}}, request.id};
                };

                nl::json paramsJson;
                if (sizeof...(Args) > 0) {

                    auto msgJson = request.toJson();
                    if (!msgJson.count("params")) {
                        //function has args since sizeof...(Args) > 0, but request doesn't have a "params" key
                        return Message{Response{Error{jrpc::invalid_params}}, request.id};
                    }
                    paramsJson = std::move(msgJson.at("params"));

                }

                try {
                    Ret &&result = func(extractSingleArg<Args>(paramsJson, argNames)...);
                    nl::json resultJson{std::move(result)}; //convert returned value to function

                    return Message{Response{std::move(resultJson)}, request.id};
                }
                catch (const jrpc::Error &error) {
                    //if a jrpc::Error is thrown from within the function, wrap it into a message and return it
                    return Message{Response{error}, request.id};
                }
            };
        }

    }

    //converts a function with signature jrpc::Return(Args...) to jrpc::Return(jrpc::Message msg) where msg.isRequest()
    //arg name strings must be passed as expected from a calling Request
    template<class FuncWithMessageArg, class FuncWithCppArgs, class ... ArgNames>
    FuncWithMessageArg wrapFunction(FuncWithCppArgs &&func, ArgNames &&... argNames) {
        //call helper that matches Func's Args
        return invoke_helpers::makeJrpcCallableHelper(func, func.operator(), std::forward<ArgNames>(argNames)...);
    }

    //tries to invoke a C++ callable with a jrpc::Message request
    //if the arguments do not match, returns an jrpc::invalid_params error
    //provide jrpc names of arguments after the functor as strings
    template<class Func, class ... ArgNames>
    Message invoke(const Message &request, Func &&func, ArgNames ... argNames) {
        //make callable and immediately invoke it
        return convertFuncToJrpcCallable(std::forward<Func>(func), std::forward<ArgNames>(argNames)...)
                (request);
    }

}}