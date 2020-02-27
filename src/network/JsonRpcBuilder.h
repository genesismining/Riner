//
//
#pragma once

#include "JsonRpcMessage.h"

namespace riner { namespace jrpc {

    /**
     * Builder pattern convenience class for creating a `jrpc::Request`
     */
    class RequestBuilder {

        Message _msg;

        Request &msgRequest();
        bool paramsIsArray();
        bool hasNoParams();

    public:
        RequestBuilder();

        /**
         * set `Message::id`
         * @return `*this` (builder pattern)
         */
        RequestBuilder &id(nl::json id);

        /**
         * set `Request::method`
         * @return `*this` (builder pattern)
         */
        RequestBuilder &method(String methodName);

        /**
         * add a named param. It is not valid to add a named param if unnamed params were added before to the same `RequestBuilder`
         * @param key name of the param
         * @param val value of the param
         * @return `*this` (builder pattern)
         */
        RequestBuilder &param(const char *key, nl::json val);

        /**
         * add an unnamed param. It is not valid to add an unnamed param if named params were added before to the same `RequestBuilder`
         * @param val value of the param
         * @return `*this` (builder pattern)
         */
        RequestBuilder &param(nl::json val);

        /**
         * call this function to end the building process and `std::move` the internal `jrpc::Message` out.
         * Afterwards, this object is in a ['valid but unspecified state'](https://en.cppreference.com/w/cpp/utility/move).
         */
        Message done();
    };

}}
