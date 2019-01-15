
#include "TcpJsonProtocolUtil.h"
#include <src/common/Json.h>

namespace miner {


    TcpJsonProtocolUtil::TcpJsonProtocolUtil(cstring_span host, cstring_span port, OnEventFunc &&onEventFunc)
    : onEvent(std::move(onEventFunc)) {

        auto onLineEvent = [this] (std::string line, auto &error, auto &coroutine) {

            bool onEventCalled = false;
            while (!onEventCalled) {//repeat until onEvent is called, so that the ioService thread is not left without work
                try {
                    if (line == "")
                        line = "{}"; //create valid json if empty string is passed
                    else {
                        LOG(INFO) << "                                   " << "<-- " << line;
                    }

                    bool allowExceptions = false;
                    auto jsonLine = nl::json::parse(line, nullptr, allowExceptions);
                    if (jsonLine.is_discarded()) {
                        LOG(ERROR) << "json got discarded: '" << line << "'";
                    } else {
                        onEventCalled = true;
                        onEvent(std::move(jsonLine), error, coroutine);
                    }
                }
                catch (nl::json::exception &e) {
                    LOG(ERROR) << "error while parsing json string: " << e.what()
                               << "\n string: " << line;
                }
                catch (std::invalid_argument &e) {
                    LOG(ERROR) << "invalid argument while parsing json string: " << e.what()
                               << "\n string: " << line;
                }
                line = ""; //retry with empty string
            }
        };

        tcpLines = std::make_unique<TcpLineProtocolUtil>(host, port, onLineEvent);
    }

    TcpJsonProtocolUtil::~TcpJsonProtocolUtil() {
        //separate definition of the destructor enables forward declaration
        //of unique_ptr<T>'s type T, since its deleter is needed only here
    }

    void TcpJsonProtocolUtil::launch() {
        tcpLines->launch();
    }

    void TcpJsonProtocolUtil::asyncWrite(nl::json request, bool reenterCoroutine) {
        asyncWrite(request.dump() + '\n', reenterCoroutine);
    }

    void TcpJsonProtocolUtil::asyncWrite(std::string request, bool reenterCoroutine) {
        LOG(INFO) << "                                   " << "--> " << request;
        tcpLines->asyncWrite(std::move(request), reenterCoroutine);
    }

    void TcpJsonProtocolUtil::asyncRead() {
        tcpLines->asyncRead();
    }

    void TcpJsonProtocolUtil::asyncRetryEvery(std::chrono::milliseconds interval, std::function<bool()> &&pred) {
        tcpLines->asyncRetryEvery(interval, std::move(pred));
    }

}
