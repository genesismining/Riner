
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
        optional<nl::json> data;
        nl::json id;

        JrpcError(Code code, cstring_span message, const optional<nl::json> &data = nullopt);
        explicit JrpcError(const nl::json &);

        nl::json getJson();
    };

    //utility class for reading/checking whether entries exist
    class JrpcResponse {
        nl::json json;
    public:
        explicit JrpcResponse(const nl::json &);
        JrpcResponse(optional<int> id, const JrpcError &, cstring_span version = "2.0");

        const nl::json &getJson() const;

        optional<JrpcError> error() const;

        optional<nl::json> result() const;

        optional<int> id() const;

        template<class T>
        bool atEquals(const char *key, const T &t) const {
            return json.count(key) && json.at(key) == t;
        }
    };

    //utility class for making json rpcs by using the builder pattern
    class JrpcBuilder {
    public:
        using ResponseFunc = std::function<void(const JrpcResponse &)>;
        using IdType = uint64_t;

        //builder pattern methods
        JrpcBuilder &method(cstring_span name);

        JrpcBuilder &param(const nl::json &val);
        JrpcBuilder &param(const char *val);
        JrpcBuilder &param(cstring_span name, const nl::json &val);

        JrpcBuilder &onResponse(ResponseFunc &&func);

        //set a custom id, if no id is set, the TcpJsonRpcProtocolUtil will choose an unused one automatically
        JrpcBuilder &id(IdType val);
        JrpcBuilder &id(optional<int> val);

        optional<IdType> getId() const;
        const nl::json &getJson() const;

        void callResponseFunc(const JrpcResponse &);

        explicit JrpcBuilder(cstring_span version = "2.0");

    private:
        ResponseFunc responseFunc;

        nl::json json;
    };
}