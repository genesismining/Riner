//
//

#include "JsonRpcMessage.h"
#include <src/util/Logging.h>

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
            id = j.at("id");

            if (j.count("method")) {
                Request req;

                req.method = j.at("method");
                req.params = j.at("params");

                var = std::move(req);
            }
            else if (j.count("result")) {
                var = Response{std::move(j.at("result"))};
            }
            else if (j.count("error")) {
                Error error;
                auto &je = j.at("error");

                error.code = toErrorCode(je.at("code"));

                if (je.count("data"))
                    error.data = std::move(je.at("data"));

                if (je.count("message")) {
                    error.message = je.at("message");
                }

                var = Response{std::move(error)};
            }
        }

        nl::json Message::toJson() const {
            nl::json j;
            j.at("id") = id;
            j.at("jsonrpc") = jsonrpc;

            var.map([&] (Request &req) {
                j.at("method") = req.method;
                j.at("params") = req.params;
            })
            .map([&] (Response &res) {

                res.var.map([&] (Error &error) {
                    j.at("error") = {};
                    auto &je = j.at("error");

                    je.at("code") = error.code;
                    je.at("data") = error.data;
                    je.at("message") = error.message;
                })
                .map([&] (nl::json &result) {
                    j.at("result") = result;
                });

            });

            return j;
        }

        Message::Message(const Response &response, nl::json id)
        : var(response), id(std::move(id)) {
        }



        bool Message::isResponse() const {
            return var.has_value<Response>({});
        }

        bool Message::isRequest() const {
            return var.has_value<Request>({});
        }
    }}