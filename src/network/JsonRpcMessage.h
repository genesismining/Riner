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
    /**
     * use `jrpc::Null{}` to express the `Null` as mentioned in the [JSON-RPC 2.0 spec](https://www.jsonrpc.org/specification#conventions) for consistency, and to avoid confusion with c++'s `0`, `NULL`, `std::nullopt`, `nullptr`
     */
    using Null = std::nullptr_t;

    /**
      * use `jrpc::String` to express the `String` as mentioned in the [JSON-RPC 2.0 spec](https://www.jsonrpc.org/specification#conventions) for consistency, and to avoid confusion with c++'s `const char *`, `string_view`, `std::string`, etc...
      */
    using String = std::string;

    /**
     * use `jrpc::Number` to express `Number`s as mentioned in the [JSON-RPC 2.0 spec](https://www.jsonrpc.org/specification#conventions) to avoid confusion with c++'s many int types.
     */
    using Number = int64_t;

    /**
     * Error codes as described in the [JSON-RPC 2.0 spec](https://www.jsonrpc.org/specification#error_object).
     */
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

    /**
     * converts `j` to a `jrpc::Number`, then calls `toErrorCode(Number)`
     * @param j
     * @return
     */
    ErrorCode toErrorCode(const nl::json &j);

    /**
     * converts `n` to a valid `ErrorCode` or `ErrorCode::error_code_unknown`. 'valid' means it's either one of the
     * explicitly named values of `ErrorCode` or a value between `ErrorCode::server_error_range_first` and `ErrorCode::server_error_range_last`.
     * @param n error code number as described in the [JSON-RPC 2.0 spec](https://www.jsonrpc.org/specification#error_object)
     * @return valid `ErrorCode` or `ErrorCode::error_code_unknown`
     */
    ErrorCode toErrorCode(Number n);

    struct Error : public std::exception { //error-specific part of response, extends std::exception so linters don't complain that it's thrown
        ErrorCode code;
        String message;
        nl::json data;

        Error(ErrorCode code = error_code_unknown, String message = "", nl::json data = {})
        : code(code), message(std::move(message)), data(std::move(data)) {
        };

        const char *what() const noexcept override {
            return message.c_str();
        };
    };

    /**
     * response-specific part of `jrpc::Message`. contains a variant that is either an `Error`
     */
    struct Response { //response-specific part of message
        variant<Error, nl::json /*= Result*/> var = Error{};
    };

    /**
     * request-specific part of `jrpc::Message`. contains name of the method as well as parameters. (`params` is `nl::json{}` if the method takes no parameters)
     */
    struct Request { //request-specific part of message
        String method;
        nl::json params;
    };

    /**
     * `jrpc::Message` class that is used to represent either a jrpc Request or Response
     */
    struct Message {
        //constexpr static auto jsonrpc = "2.0"; //always the same
        variant<Request, Response> var = Request{};

        /**
         * `id`, ideally `Null`, `String`, `Number` or empty (=`nl::json{}`). Empty means that no `id` member is
         * present altogether and therefore the `Message` is a notification (see `isNotification()`)
         */
        nl::json id; //ideally Null, String or Number, empty when notification

        Message() = default;

        /**
         * creates a `jrpc::Message` by parsing an `nl::json`. Throws the [exceptions of nlohmann::json::at](https://nlohmann.github.io/json/classnlohmann_1_1basic__json_acac9d438c9bb12740dcdb01069293a34.html) on parsing failure.
         */
        explicit Message(nl::json); //throws on parser failure

        /**
         * convenience ctor for creating a response `jrpc::Message` from a `jrpc::Response` and optionally an `id`.
         * @param id empty by default, sets the `id` member of the constructed `jrpc::Message`
         */
        explicit Message(const Response &, nl::json id = {});//convenience constructor for making a response message

        /**
         * creates a `nl::json` that corresponds to this `jrpc::Message` object. Use `jrpc::Message::str()` instead if you want to directly convert the object to `std::string`
         */
        nl::json toJson() const;

        /**
         * whether this `Message` is a notification, that is, a `Response` with no `id` (`id == nl::json{}`)
         */
        bool isNotification() const;

        /**
         * whether this `Message` is a `Request` as opposed to being a `Response`
         */
        bool isRequest () const;

        /**
         * whether this `Message` is a `Response` as opposed to being a `Request`
         */
        bool isResponse() const;

        /**
         * convenience function for quick access to the `jrpc::Error`.
         * checks if this `Message` is a `jrpc::Response`, then checks whether it is a `jrpc::Error`. If it is, returns the ref to that `jrpc::Error`
         * @return ref to the underlying `jrpc::Error` or `nullopt`
         */
        optional_cref<Error> getIfError() const;

        /**
         * convenience function for quick access to the result `nl::json` object, if this `Message` is a `Request` that is not an `Error`.
         * @return ref to underlying result `nl::json` object, or `nullopt`
         */
        optional_ref<nl::json> getIfResult(); //TODO: make const

        /**
         * convenience function for quick access to the `Request` object.
         * @return ref to this `Message`'s `jrpc::Request` object if it is a request, `nullopt` otherwise.
         */
        optional_ref<Request> getIfRequest(); //TODO: make const

        /**
         * convenience function that checks whether this `Message` is a `Response` that is not an `Error`. If so, tries to
         * convert the `"result"` object to `T` and return it.
         * @tparam T the type that the `"result"` should be converted to
         * @return the `jrpc::Response`s `"result"` as a `T`, or `nullopt`
         */
        template<class T>
        optional<T> resultAs() {
            if (optional_ref<nl::json> result = getIfResult()) {
                try {
                    return result->get<T>();
                }
                catch (const nl::json::exception &e) {}
            }
            return nullopt;
        }

        /**
         * returns whether this `Message` is a `Response` with a `"result" = True` entry.
         * this is needed so often that it deserves its own convenience function. This function is safe to be called
         * even if this `Message` is no `Response` or does not have a `"result"` key.
         * @return `true` if this `Message` is a `Response` with a `"result" = True` entry, `false` otherwise.
         */
        bool isResultTrue(); //TODO: make const

        /**
         * returns whether this `Message` is a `Response` that is an `Error`
         */
        bool isError() const;

        /**
         * returns whether this `Message` is a `Requeset` that has the same `method` name as the provided `name` argument.
         */
        bool hasMethodName(const char *name) const; //returns whether the message is a request and has the same method name as provided

        /**
         * convert the `jrpc::Message` object to a single-line `std::string` ready to be sent. For nicer formatting use `nlohmann::json::dump()` functionality and call `jrpc::Message::toJson().dump(4)` instead.
         * @return
         */
        std::string str() const;
    };

}}
