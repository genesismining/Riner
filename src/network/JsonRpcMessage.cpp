//
//

#include "JsonRpcMessage.h"
#include <src/util/Logging.h>
#include <src/common/Assert.h>
#include <type_safe/visitor.hpp>

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
            return var.has_value<Response>({});
        }

        bool Message::isRequest() const {
            return var.has_value<Request>({});
        }

        optional_ref<const Error> Message::getIfError() const {
            optional_ref<const Error> result;
            visit<Response>(var, [&] (auto &r) {
                visit<Error>(r.var, [&] (auto &err) {
                    result = type_safe::opt_cref(err);
                });
            });
            return result;
        }

        optional_ref<nl::json>
        Message::getIfResult() {
            optional_ref<nl::json> result;
            visit<Response>(var, [&] (auto &r) {
                visit<nl::json>(r.var, [&] (auto &res) {
                    result = type_safe::opt_ref(res);
                });
            });
            return result;
        }

        optional_ref<Request> Message::getIfRequest() {
            optional_ref<Request> result;
            visit<Request>(var, [&] (auto &req) {
                result = type_safe::opt_ref(req);
            });
            return result;
        }

        bool Message::isResultTrue() {
            return resultAs<bool>().value_or(false);
        }

        bool Message::hasMethodName(const char *name) const {
            bool result = false;
            visit<Request>(var, [&] (const Request &req) {
                result = req.method == name;
            });
            return result;
        }

        bool Message::isError() const {
            return getIfError().has_value();
        }

        std::string Message::str() const {
            return toJson().dump();
        }

    }}
