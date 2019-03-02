
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
        nl::json errorJson; //full json

        JrpcError(Code code, cstring_span message, const optional<nl::json> &data = nullopt);;
        explicit JrpcError(const nl::json &);
    };

    //utility class for reading/checking whether entries exist
    class JrpcResponse {
        nl::json json;
    public:
        using IdType = uint64_t;
        explicit JrpcResponse(const nl::json &);
        JrpcResponse(optional<IdType> id, const JrpcError &, cstring_span version = "2.0");

        const nl::json &getJson() const;

        optional<JrpcError> error() const;

        optional<nl::json> result() const; //returns empty json if error

        optional<IdType> id() const;

        template<class T>
        bool atEquals(const char *key, const T &t) const {
            return json.count(key) && json.at(key) == t;
        }
    };

    //utility class for making json rpcs by using the builder pattern
    class JrpcBuilder {
    public:
        using ResponseFunc = std::function<void(const JrpcResponse &)>;
        using IdType = JrpcResponse::IdType;

        struct Options {
            bool serializeIdAsString = false;
            cstring_span version = "2.0";
        };

        JrpcBuilder(): JrpcBuilder(Options()) {}
        explicit JrpcBuilder(Options options): options_(std::move(options)) {
            json["jsonrpc"] = gsl::to_string(options.version);
        }

        //builder pattern methods
        JrpcBuilder &method(cstring_span name);

        JrpcBuilder &param(const nl::json &val);
        JrpcBuilder &param(const char *val);
        JrpcBuilder &param(cstring_span name, const nl::json &val);

        JrpcBuilder &onResponse(ResponseFunc &&func);

        //set a custom id, if no id is set, the TcpJsonRpcProtocolUtil will choose an unused one automatically
        JrpcBuilder &setId(IdType val);
        JrpcBuilder &setId(optional<IdType> val);

        optional<IdType> getId() const;
        const nl::json &getJson() const;

        void callResponseFunc(const JrpcResponse &);

    private:
        Options options_;

        ResponseFunc responseFunc;

        nl::json json;
    };
}
