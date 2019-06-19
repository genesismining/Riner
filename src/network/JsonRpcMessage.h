//
//

#pragma once

#include <string>
#include <src/common/Json.h>
#include <src/common/Variant.h>
#include <src/common/Optional.h>
#include <src/common/Pointers.h>
#include <src/util/Logging.h>

namespace miner { namespace jrpc {

    //typedefs for mapping more directly to the Json Rpc spec
    using Null   = std::nullptr_t;
    using String = std::string;
    using Number = int64_t;

    enum ErrorCode {
        error_code_unknown = 0,

        parse_error      = -32700,
        invalid_request  = -32600,
        method_not_found = -32601,
        invalid_params   = -32602,
        internal_error   = -32603,

        server_error_range_first = -32099,
        server_error_range_last  = -32000,
    };

    ErrorCode toErrorCode(const nl::json &j);
    ErrorCode toErrorCode(int64_t n);

    struct Error { //error-specific part of response
        ErrorCode code = error_code_unknown;
        String message;
        nl::json data;
    };

    struct Response { //response-specific part of message
        variant<Error, nl::json /*= Result*/> var = Error{};
    };

    struct Request { //request-specific part of message
        String method;
        nl::json params;
    };

    struct Message {
        //constexpr static auto jsonrpc = "2.0"; //always the same
        variant<Request, Response> var = Request{};
        nl::json id; //ideally Null, String or Number, empty when notification

        Message() = default;
        explicit Message(nl::json); //throws on parser failure
        explicit Message(const Response &, nl::json id = {});//convenience constructor for making a response message
        nl::json toJson() const;

        bool isNotification() const;
        bool isRequest () const;
        bool isResponse() const;

        optional_ref<Error> getIfError() const;
        optional_ref<nl::json> getIfResult();
        optional_ref<Request> getIfRequest();

        template<class T>
        optional<T> resultAs() {
            if (optional_ref<nl::json> result = getIfResult()) {
                try {
                    return result.value().get<T>();
                }
                catch (const nl::json::exception &e) {}
            }
            return nullopt;
        }

        //returns whether this message is a Response with a "result" = true entry
        bool isResultTrue(); //this is needed so often that it deserves its own convenience function

        bool isError() const;

        bool hasMethodName(const char *name) const;

        std::string str() const;
    };

    using MessageBatch = std::vector<Message>;

}}
