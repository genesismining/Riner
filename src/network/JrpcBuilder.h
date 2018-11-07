
#pragma once

#include <src/common/StringSpan.h>
#include <src/common/Json.h>
#include <src/common/Optional.h>

namespace miner {

    struct JrpcError {
        enum Code {
            uninitialized = 0,

            parse_error = -32700,
            invalid_request = -32600,
            method_not_found = -32601,
            invalid_params = -32602,
            internal_error = -32603,
        };
        Code code = uninitialized;
        std::string message;
        nl::json data;
        nl::json errorJson;

        JrpcError(const nl::json &);
    };

    //utility class for reading/checking whether entries exist
    class JrpcResponse {
        nl::json json;
    public:
        explicit JrpcResponse(const nl::json &);

        const nl::json &getJson() const;

        optional<JrpcError> error() const;

        nl::json result() const; //returns empty json if error

        optional<int> id() const;

        template<class T>
        bool safeAtEquals(const char *key, const T &t) const {
            return json.count(key) && json.at(key) == t;
        }
    };

    //utility class for making json rpcs by using the builder pattern
    class JrpcBuilder {
    public:
        using ResponseFunc = std::function<void(const JrpcResponse &)>;

        //builder pattern methods
        JrpcBuilder &method(cstring_span name);

        JrpcBuilder &param(cstring_span val);
        JrpcBuilder &param(const nl::json &val);
        JrpcBuilder &param(cstring_span name, const nl::json &val);

        JrpcBuilder &onResponse(ResponseFunc &&func);

        //set a custom id, if no id is set, the TcpJsonRpcProtocolUtil will choose an unused one automatically
        JrpcBuilder &id(int val);

        optional<int> getId() const;
        const nl::json &getJson() const;

        void callResponseFunc(const JrpcResponse &);

    private:
        JrpcBuilder(cstring_span version = "2.0");

        ResponseFunc responseFunc;

        nl::json json;
        optional<int> rpcId;
    };
}