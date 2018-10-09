
#include "TcpLineSubscription.h"

#include <asio.hpp>
#include <thread>

namespace miner {

    using asio::ip::tcp;
    using asio::error_code;

    TcpLineSubscription::TcpLineSubscription(cstring_span hostSpan,
            cstring_span portSpan,
            OnSendMessageFunc &&getSubscriptionMessageFunc,
            OnReceivedLineFunc &&onReceivedLineFunc)
    : host(to_string(hostSpan))
    , port(to_string(portSpan))
    , getSubscriptionMessage(std::move(getSubscriptionMessageFunc))
    , onReceivedLine(std::move(onReceivedLineFunc))
    , resolver(ioService)
    , socket(ioService) {

        tcp::resolver::query query(host, port);

        //start chain of async calls
        resolver.async_resolve(query, [this] (auto &error, auto it) {
            handleResolve(error, it);
        });

        //start io service thread which will handle the async calls
        thread = std::make_unique<std::thread>([this] () {
            try {
                ioService.run();
            }
            catch(std::exception &e) {
                LOG(ERROR) << e.what();
            }
        });
    }

    TcpLineSubscription::~TcpLineSubscription() {
        ioService.stop();

        if (thread->joinable()) {
            thread->join();
        }
    }

    void TcpLineSubscription::retry() {
        LOG(INFO) << "trying to reconnect";

        if (socket.is_open()) {
            socket.close();
        }

        tcp::resolver::query query(host, port);

        resolver.async_resolve(query, [this] (auto &error, auto it) {
            handleResolve(error, it);
        });
    }

    void reportAsioError(const error_code &error) {
        LOG(ERROR) << error.message();
    }

    void TcpLineSubscription::handleResolve(const error_code &error, tcp::resolver::iterator it) {
        if (!error) {
            //try connect to first endpoint, handleConnect tries the other endpoints if that fails
            tcp::endpoint endpoint = *it;
            socket.async_connect(endpoint, [this, it] (auto &error) {
                handleConnect(error, it);
            });
        }
        else {
            reportAsioError(error);
            retry();
        }
    }

    void TcpLineSubscription::handleConnect(const error_code &error, tcp::resolver::iterator it) {
        if (!error) {
            //successfully connected, send subscribe message
            std::ostream stream(&request);

            getSubscriptionMessage(stream);

            asio::async_write(socket, request, [this] (auto &error, size_t numBytes) {
                handleSubscribe(error, numBytes);
            });
        }
        else if (it != tcp::resolver::iterator()) {//default constructed iterator is "end"
            //The connection failed, but there's another endpoints to try.
            socket.close();
            tcp::endpoint endpoint = *it;
            socket.async_connect(endpoint, [this, next = ++it] (auto &error) {
                handleConnect(error, next);
            });
        }
        else {
            reportAsioError(error);
            retry();
        }
    }

    void TcpLineSubscription::handleSubscribe(const error_code &error, size_t numBytes) {
        if (!error) {
            //read until '\n'
            asio::async_read_until(socket, response, "\n", [this] (auto &error, size_t numBytes) {
                handleReadLine(error, numBytes);
            });
        }
        else {
            reportAsioError(error);
            retry();
        }
    }

    void TcpLineSubscription::handleReadLine(const error_code &error, size_t numBytes) {
        if (!error) {
            std::istream stream(&response);
            std::string line;
            std::getline(stream, line);

            LOG(INFO) << "response (" << numBytes << " bytes): " << line;

            onReceivedLine(line);

            // Read the response headers, which are terminated by a blank line.
            asio::async_read_until(socket, response, "\n", [this] (auto &error, size_t numBytes) {
                handleReadLine(error, numBytes);
            });
        }
        else {
            reportAsioError(error);
            retry();
        }
    }


}