//
//

#include "JsonRpcMessage.h"
#include <src/util/Logging.h>
#include <src/common/Assert.h>

namespace miner { namespace jrpc {

        ErrorCode toErrorCode(const nl::json &j) {
            if (j.is_number_integer())
                return toErrorCode(j.get<int64_t>());

            return error_code_unknown;
        }

        ErrorCode toErrorCode(int64_t n) {
            auto e = static_cast<ErrorCode>(n);

            if (n >= server_error_range_first &&
                n <= server_error_range_last)
                return e;

            switch(e) {
                case parse_error:
                case invalid_request:
                case method_not_found:
                case invalid_params:
                case internal_error:
                    return e;
                default: {
                    LOG(INFO) << "unknown jrpc error code " << n;
                    return error_code_unknown;
                }
            }
        }

        Message::Message(nl::json j) {
            auto idIt = j.find("id");
            if (idIt != j.end()) {
                id = *idIt;
            }

            auto resultIt = j.find("result");
            auto errorIt = j.find("error");
            // Bitcoin stratum protocol responses include "error" and "result" keys always
            if (errorIt != j.end() && !errorIt->is_null()) {
                Error error;
                auto &je = *errorIt;

                if (je.is_array()) {
                    // Bitcoin stratum protocol error responses contain an "error" array
                    error.code = toErrorCode(je.at(0));
                    error.message = std::move(je.at(1));
                }
                else {
                    error.code = toErrorCode(je.at("code"));

                    if (je.count("data"))
                        error.data = std::move(je.at("data"));

                    if (je.count("message")) {
                        error.message = std::move(je.at("message"));
                    }
                }

                var = Response{std::move(error)};
            }
            else if (resultIt != j.end()) {
                var = Response{std::move(*resultIt)};
            }
            else {
                // message is not a Response, therefore it has to be a Request
                Request req;

                req.method = std::move(j.at("method"));
                auto paramsIt = j.find("params");
                if (paramsIt != j.end())
                    req.params = *paramsIt;

                var = std::move(req);
            }
        }

        nl::json Message::toJson() const {
            nl::json j;
            j["id"] = id;
            j["jsonrpc"] = "2.0";

            visit<Request>(var, [&] (const Request &req) {
                j["method"] = req.method;
                j["params"] = req.params;
            });
            visit<Response>(var, [&] (const Response &res) {

                visit<Error>(res.var, [&] (const Error &error) {
                    j["error"] = {};
                    auto &je = j.at("error");

                    je["code"] = error.code;
                    je["data"] = error.data;
                    je["message"] = error.message;
                });
                visit<nl::json>(res.var, [&] (const nl::json &result) {
                    j["result"] = result;
                });

            });

            return j;
        }

        Message::Message(const Response &response, nl::json id)
        : var(response), id(std::move(id)) {
        }

        bool Message::isNotification() const {
            return isRequest() && id.empty();
        }

        bool Message::isResponse() const {
            return mp::holds_alternative<Response>(var);
        }

        bool Message::isRequest() const {
            return mp::holds_alternative<Request>(var);
        }

        optional_cref<Error> Message::getIfError() const {
            if (auto respPtr = mp::get_if<Response>(&var)) {
                if (auto errPtr = mp::get_if<Error>(&respPtr->var)) {
                    return *errPtr;
                }
            }
            return {};
        }

        optional_ref<nl::json>
        Message::getIfResult() {
            if (auto respPtr = mp::get_if<Response>(&var)) {
                if (auto resPtr = mp::get_if<nl::json>(&respPtr->var)) {
                    return *resPtr;
                }
            }
            return {};
        }

        optional_ref<Request> Message::getIfRequest() {
            if (auto reqPtr = mp::get_if<Request>(&var)) {
                return *reqPtr;
            }
            return {};
        }

        bool Message::isResultTrue() {
            return resultAs<bool>().value_or(false);
        }

        bool Message::hasMethodName(const char *name) const {
            if (auto reqPtr = mp::get_if<Request>(&var)) {
                return reqPtr->method == name;
            }
            return false;
        }

        bool Message::isError() const {
            return getIfError().has_value();
        }

        std::string Message::str() const {
            return toJson().dump();
        }

    }}
