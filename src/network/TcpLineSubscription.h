
#pragma once

#include <src/common/StringSpan.h>
#include <src/common/Pointers.h>
#include <src/util/Logging.h>
#include <asio.hpp>
#include <functional>

namespace miner {

    class TcpLineSubscription {
    public:
        using tcp = asio::ip::tcp;

        using OnReceivedLineFunc = std::function<void(cstring_span line)>;
        using GetSubscriptionMessageFunc = std::function<void (std::ostream &)>;

        TcpLineSubscription(cstring_span host, cstring_span port,
                GetSubscriptionMessageFunc &&, OnReceivedLineFunc &&);

        ~TcpLineSubscription();

    private:
        std::string host;
        std::string port;

        GetSubscriptionMessageFunc getSubscriptionMessage;
        OnReceivedLineFunc onReceivedLine;

        unique_ptr<std::thread> thread;
        asio::io_service ioService;

        tcp::resolver resolver;
        tcp::socket socket;

        asio::streambuf request;
        asio::streambuf response;

        void retry();

        void handleResolve(const asio::error_code &error, tcp::resolver::iterator it);

        void handleConnect(const asio::error_code &error, tcp::resolver::iterator it);

        void handleSubscribe(const asio::error_code &error, size_t numBytes);

        void handleReadLine(const asio::error_code &error, size_t numBytes);

    };

}