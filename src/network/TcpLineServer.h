
#pragma once

#include <asio.hpp>
#include <lib/asio/asio/include/asio/ip/tcp.hpp>
#include <src/common/Pointers.h>

namespace miner {

    class TcpLineConnection {
    public:
        using OnEventFunc = std::function<void(const std::string&, TcpLineConnection &)>;
    private:
        using tcp = asio::ip::tcp;

        unique_ptr<tcp::socket> socket;
        std::shared_ptr<OnEventFunc> onEvent;

        asio::streambuf request;

        friend class TcpLineServer;

        void start(std::shared_ptr<TcpLineConnection> sharedPtrToThis);

        void listen(std::shared_ptr<TcpLineConnection> sharedThis);
    public:

        explicit TcpLineConnection(unique_ptr<tcp::socket> socket);

        void asyncWrite(std::string response);
    };

    class TcpLineServer {
    public:
        using tcp = asio::ip::tcp;

        using OnEventFunc = TcpLineConnection::OnEventFunc;

        explicit TcpLineServer(uint64_t port, OnEventFunc func);
        ~TcpLineServer();

        template<class Fn>
        void postAsync(Fn &&func) {
            asio::post(ioService, std::forward<Fn>(func));
        }

        void launch();

    private:
        void listen();

        asio::io_service ioService;

        tcp::acceptor acceptor;
        std::shared_ptr<OnEventFunc> onEvent; //shared with instances of TcpLineConnection

        std::atomic_bool shutdown {false};

        unique_ptr<std::thread> thread;
    };

}