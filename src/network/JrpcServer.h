
#pragma once

#include <src/common/Json.h>
#include <src/common/Pointers.h>
#include <src/common/Optional.h>
#include <src/common/Variant.h>
#include "TcpLineServer.h"
#include "JrpcBuilder.h"

namespace miner {

    class JrpcReturn { //return value of jrpc function (the "error" or "result" part of the response)
        nl::json value;
        std::string key = "error"; //"error" or "result"
    public:
        JrpcReturn(nl::json result);
        JrpcReturn(JrpcError error);

        const nl::json &getValue() const;
        std::string getKey() const;

        //convenience constructor: if it starts with an error code, it's an error
        template<class ... Ts>
        JrpcReturn(JrpcError::Code code, Ts && ...ts)
                : JrpcReturn(JrpcError{code, std::forward<Ts>(ts) ...}) {}

        //convenience constructor: if its neither a json or an error, try to convert the type to a json
        template<class T>
        JrpcReturn(const T &t) : JrpcReturn(nl::json{t}) {
            static_assert(!std::is_same<T, JrpcError::Code>::value, "");
        }
    };

    class JrpcServer {
    public:

        struct JrpcFunction {
            std::string methodName;
            std::function<JrpcReturn(const nl::json &)> callable;
        };

        explicit JrpcServer(uint64_t port);

        template<class Func, class ... ArgNames>
        void registerFunc(const std::string &method, Func func, ArgNames ... argNames) {
            addFunctionWrapper(method, func, &Func::operator(), std::move(argNames)...);
        }

        void launch();

    private:
        std::vector<JrpcFunction> funcs;
        unique_ptr<TcpLineServer> tcpLines;

        optional_ref<const JrpcFunction> funcWithName(const std::string &name) const;

        void registerJrpcFunc(JrpcFunction func);

        template<class T>
        T parseSingleArg(const nl::json &jArgs, const std::string &argName) {
            return jArgs.at(argName).get<T>();
        }

        //this method is used to match Func's callOperator, so that the args types can be extracted
        template<class Func, class Ret, class Cls, class ... Args, class ... ArgNames>
        void addFunctionWrapper(const std::string &method, Func func,
                                Ret(Cls::* funcCallOperator)(Args...) const, ArgNames ... argNames) {

            static_assert(sizeof...(Args) == sizeof...(ArgNames), "amount of args of function doesn't match amount of argNames, provide a name string for every argument!");

            //create a wrapper that converts the Ret(Args...) function to a JsonResponse(const nl::json &) function
            registerJrpcFunc({method, [this, func = std::move(func), argNames...] (const nl::json &argsJson) {
                (void)this; //silence 'unused this capture' warning
                return func(parseSingleArg<Args>(argsJson, argNames)...);
            }});
        }

        void onEvent(const std::string &line, TcpLineConnection &conn) const;

        bool parseLine(const std::string &line, std::string &outVersion, nl::json &outId, std::string &outMethodName, nl::json &outArgs) const;

    };

}