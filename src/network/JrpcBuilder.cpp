
#include <src/common/Assert.h>
#include "JrpcBuilder.h"

namespace miner {

    JrpcError::JrpcError(const nl::json &j) {
        if (j.count("code"))
            code = j.at("code");
        if (j.count("message"))
            message = j.at("message");
        if (j.count("data"))
            data = j.at("data");

        errorJson = j;
    }

    JrpcBuilder::JrpcBuilder(cstring_span version) {
        json["jsonrpc"] = gsl::to_string(version);
    }

    JrpcBuilder &JrpcBuilder::method(cstring_span name) {
        json["method"] = gsl::to_string(name);
        return *this;
    }

    JrpcBuilder &JrpcBuilder::param(const nl::json &val) {
        MI_EXPECTS(!json.count("params") || !json.at("params").is_object()); //did you add a named param before and are now adding an unnamed one?

        auto &jparams = json["params"];
        jparams.push_back(val);
        return *this;
    }

    JrpcBuilder &JrpcBuilder::param(cstring_span name, const nl::json &val) {
        MI_EXPECTS(!json.count("params") || !json.at("params").is_array()); //did you add an unnamed param before and are now adding a named one?

        auto &jparams = json["params"];
        jparams[gsl::to_string(name)] = val;
        return *this;
    }

    JrpcBuilder &JrpcBuilder::param(const char *val) {
        return param(static_cast<const nl::json &>(std::string(val)));
    }

    JrpcBuilder &JrpcBuilder::onResponse(ResponseFunc &&func) {
        responseFunc = std::move(func);
        return *this;
    }

    optional<int> JrpcBuilder::getId() const {
        //todo: decide whether to handle "null" id differently
        if (json.count("id")) {
            auto &jid = json.at("id");
            if (jid.is_number())
                return jid.get<int>();
        }
        return nullopt;
    }

    JrpcBuilder &JrpcBuilder::id(int val) {
        json["id"] = val;
        return *this;
    }

    const nl::json &JrpcBuilder::getJson() const {
        return json;
    }

    void JrpcBuilder::callResponseFunc(const JrpcResponse &response) {
        responseFunc(response);
    }

    JrpcResponse::JrpcResponse(const nl::json &j)
    : json(j) {
    }

    const nl::json &JrpcResponse::getJson() const {
        return json;
    }

    optional<int> JrpcResponse::id() const {
        if (json.count("id"))
            return json.at("id").get<int>();
        return nullopt;
    }

    optional<nl::json> JrpcResponse::result() const {
        if (json.count("result"))
            return {json.at("result")};
        return nullopt;
    }

    optional<JrpcError> JrpcResponse::error() const {
        if (json.count("error"))
            return {json.at("error")};
        return nullopt;
    }

}