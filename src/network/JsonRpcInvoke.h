//
//

#pragma once

#include "JsonRpcMessage.h"
#include <src/util/TemplateUtils.h>
#include <src/common/Assert.h>

namespace riner { namespace jrpc {

    namespace detail {

        template<class T>
        T extractSingleArg(const nl::json &jArgs, const std::string &argName) {
            if (!jArgs.count(argName))
                throw jrpc::Error{ErrorCode::invalid_params, std::string() + "missing argument '" + argName + "'"};
            return jArgs.at(argName).get<T>();
        }

        template<class Ret>
        struct CallFuncAndReturnMessage { //this template exists so that void can be handled differently. in c++14 we don't have "if constexpr"
            template<class Func, class...Args>
            static Message invoke(Func &&func, Args &&... args) {
                nl::json resultJson = func(std::forward<Args>(args)...); //convert returned value to json
                return Message{Response{std::move(resultJson)}};
            }
        };

        template<>
        struct CallFuncAndReturnMessage<void> { //specialization for void return type
            template<class Func, class...Args>
            static Message invoke(Func &&func, Args &&... args) {
                func(std::forward<Args>(args)...);
                return Message{Response{nl::json{}}}; //return empty json
            }
        };

        //this method is used to match Func's callOperator, so that the args types can be extracted
        template<class Func, class Ret, class Cls, class ... Args, class ... ArgNames>
        auto makeJrpcCallableHelper(Func func, Ret(Cls::* funcCallOperator)(Args...) const, ArgNames &&... argNames) {

            //constexpr bool argsIsOneSingleJson = sizeof...(Args) == 1 && firstIsTrue<std::is_same<nl::json, std::decay_t<Args>>::value ...>::value;
            constexpr bool asManyArgNamesAsArgs = sizeof...(Args) == sizeof...(ArgNames);

            static_assert(asManyArgNamesAsArgs
                            , "amount of args of function doesn't match amount of argNames, provide a name string for every argument! Don't provide more strings than arguments! If you want to accept a json array as argument, use nl::json as the only argument");

            //create a wrapper that converts the Ret(Args...) function to a JsonResponse(const nl::json &) function
            return [func = std::move(func), argNames...] (const jrpc::Message &request) -> jrpc::Message {

                if (!request.isRequest()) {
                    //function was called with a message that is not a request
                    return Message{Response{Error{jrpc::invalid_request}}, request.id};
                };

                nl::json paramsJson;
                if (sizeof...(Args) > 0) {

                    auto msgJson = request.toJson();
                    if (!msgJson.count("params")) { //in the current implementation a messsage json created from request.toJson() always contains "params" so this check is not needed
                        //function has args since sizeof...(Args) > 0, but request doesn't have a "params" key
                        return Message{Response{Error{jrpc::invalid_params}}, request.id};
                    }
                    paramsJson = std::move(msgJson.at("params"));

                }

                try {
                    Message response = CallFuncAndReturnMessage<Ret>::invoke(std::move(func), extractSingleArg<Args>(paramsJson, argNames)...);
                    response.id = request.id;
                    return response;
                }
                catch (const jrpc::Error &error) {
                    //if a jrpc::Error is thrown from within the function, wrap it into a message and return it
                    return Message{Response{error}, request.id};
                }
            };
        }

        //this method is used to match Func's callOperator in case it only takes one nl::json
        template<class Func, class Ret, class Cls>
        auto makeJrpcCallableHelper(Func func, Ret(Cls::* funcCallOperator)(nl::json) const) {

            //create a wrapper that converts the Ret(Args...) function to a JsonResponse(const nl::json &) function
            return [func = std::move(func)] (const jrpc::Message &request) -> jrpc::Message {

                if (!request.isRequest()) {
                    //function was called with a message that is not a request
                    return Message{Response{Error{jrpc::invalid_request}}, request.id};
                };

                nl::json paramsJson;
                {
                    auto msgJson = request.toJson();
                    if (!msgJson.count("params")) {
                        //function has args since sizeof...(Args) > 0, but request doesn't have a "params" key
                        return Message{Response{Error{jrpc::invalid_params}}, request.id};
                    }
                    paramsJson = std::move(msgJson.at("params"));
                }

                try {
                    Message response = CallFuncAndReturnMessage<Ret>::invoke(std::move(func), paramsJson);
                    response.id = request.id;
                    return response;
                }
                catch (const jrpc::Error &error) {
                    //if a jrpc::Error is thrown from within the function, wrap it into a message and return it
                    return Message{Response{error}, request.id};
                }
            };
        }
    }

    //used for wrapping C++ functions with actual typed arguments, so that it can be called via jrpc::Message
    using WrappedFunc = std::function<Message(const Message &request)>;

    //converts a C++ callable T(Rs...) to a function jrpc::Message(jrpc::Message request)
    //arg name strings must be passed the way they are expected from a calling Request.
    //if the C++ callable takes a single nl::json as an argument and no argname is provided, the contents of the request's "params" key will be passed into the invocation directly (without further parasing).
    template<class FuncWithCppArgs, class ... ArgNames>
    auto wrapFunc(FuncWithCppArgs &&func, ArgNames &&... argNames) {
        //call helper that matches Func's Args
        return detail::makeJrpcCallableHelper(std::forward<FuncWithCppArgs>(func), &FuncWithCppArgs::operator(), std::forward<ArgNames>(argNames)...);
    }

    //tries to invoke a C++ callable with a jrpc::Message request
    //if the arguments do not match, returns an jrpc::invalid_params error
    //provide jrpc names of arguments after the functor as strings
    template<class Func, class ... ArgNames>
    Message invoke(const Message &request, Func &&func, ArgNames ... argNames) {
        //wrap func and immediately invoke it
        return wrapFunc(std::forward<Func>(func), std::forward<ArgNames>(argNames)...)
                (request);
    }

}}
