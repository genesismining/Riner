//
//

#include <src/common/Assert.h>
#include "JrpcServer.h"

namespace miner {

    void JrpcServer::registerJrpcFunc(JrpcFunction func) {
        MI_EXPECTS(!funcWithName(func.methodName)); //function with this name already exists
        funcs.push_back(std::move(func));
    }

    optional_ref<const JrpcServer::JrpcFunction> JrpcServer::funcWithName(const std::string &name) const {
        for (auto &func : funcs) {
            if (func.methodName == name)
                return type_safe::opt_ref(func);
        }
        return nullopt;
    }

    void JrpcServer::launch() {
        tcpLines->launch();
    }

    void JrpcServer::onEvent(const std::string &line, TcpLineConnection &conn) const {

        std::string version;
        nl::json id;
        std::string method;
        nl::json args;

        optional<JrpcReturn> jreturn;

        if (parseLine(line, version, id, method, args)) {

            if (auto funcOr = funcWithName(method)) {
                auto &func = funcOr.value();

                try {
                    jreturn = func.callable(args);
                }
                catch (nl::json::exception &e) {
                    std::string errStr =
                            "json exception while running Jrpc method '" + func.methodName + ": " + e.what();
                    LOG(ERROR) << errStr << "\nline string: " << line;
                    jreturn = JrpcError{JrpcError::internal_error, errStr};
                }
                catch (std::invalid_argument &e) {
                    std::string errStr =
                            "invalid argument while running Jrpc method '" + func.methodName + "': " + e.what();
                    LOG(ERROR) << errStr << "\nline string: " << line;
                    jreturn = JrpcError{JrpcError::invalid_params, errStr};
                }

            } else {
                std::string errStr = "no method with name " + method;
                jreturn = JrpcError{JrpcError::method_not_found, errStr};
            }

        }

        if (jreturn) {
            auto key = jreturn.value().getKey();
            auto &value = jreturn.value().getValue();

            nl::json response = {
                    {"jsonrpc", version},
                    {"id",      id},
                    {key,       value}
            };

            conn.asyncWrite(response.dump());
        }
    }

    JrpcServer::JrpcServer(uint64_t port) {
        //set onEvent function to be called whenever a line is received.
        //tcpLines is a unique_ptr because the 'this' pointer is captured,
        //'this' should not be passed in the constructor's initializer list
        tcpLines = std::make_unique<TcpLineServer>(port, [this] (auto &line, auto &connection) {
            onEvent(line, connection);
        });
    }

    bool JrpcServer::parseLine(const std::string &line, std::string &outVersion,
            nl::json &outId, std::string &outMethodName, nl::json &outArgs) const {
        try {
            auto j = nl::json::parse(line);

            outVersion = j.at("jsonrpc");
            outId = j.at("id");
            outMethodName = j.at("method");
            outArgs = j.at("params");

            return true;
        }
        catch (nl::json::exception &e) {
            LOG(ERROR) << "error while parsing jrpc method string: " << e.what()
                       << "\n string: " << line;
        }
        catch (std::invalid_argument &e) {
            LOG(ERROR) << "invalid argument while parsing jrpc method string: " << e.what()
                       << "\n string: " << line;
        }
        return false;
    }

    JrpcReturn::JrpcReturn(nl::json result)
    : value(std::move(result))
    , key("result") {
    }

    JrpcReturn::JrpcReturn(JrpcError error)
    : value(error.getJson())
    , key("error") {
    }

    const nl::json &JrpcReturn::getValue() const {
        return value;
    }

    std::string JrpcReturn::getKey() const {
        return key;
    }
}