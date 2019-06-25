
#pragma once

#include <asio/coroutine.hpp>

#include <src/common/StringSpan.h>
#include <src/common/Pointers.h>
#include <src/util/Logging.h>
#include <asio.hpp>
#include <functional>
#include <src/util/LockUtils.h>
#include <list>

namespace miner {

    class TcpLineProtocolUtil {
    public:
        using tcp = asio::ip::tcp;

        using OnEventFunc = std::function<void(std::string responseLine, const asio::error_code &, asio::coroutine &)>;

        TcpLineProtocolUtil(cstring_span host, uint16_t port, OnEventFunc &&onEvent);
        ~TcpLineProtocolUtil();

        template<class Fn>
        void postAsync(Fn &&func) {
            asio::post(ioService, std::forward<Fn>(func));
        }

        void launch();
        void asyncWrite(std::string request, bool reenterCoroutine = false);
        void asyncRead();

        //retries calling the function after a time interval until it returns true for the first time
        //first call is scheduled immediately (via postAsync);
        void asyncRetryEvery(std::chrono::milliseconds interval, std::function<bool()> &&pred);

    private:
        std::string host;
        uint16_t port;

        OnEventFunc onEvent;

        std::atomic_bool shutdown {false};

        unique_ptr<std::thread> thread;
        asio::io_service ioService;

        tcp::resolver resolver;
        tcp::socket socket;

        asio::streambuf request;
        asio::streambuf response;

        struct AsyncRetry;
        LockGuarded<std::list<std::shared_ptr<AsyncRetry>>> activeRetries;

        using WaitHandler = std::function<void(const asio::error_code &)>;

        struct AsyncRetry {
            asio::steady_timer timer;
            std::function<bool()> pred;
            WaitHandler waitHandler;
            std::list<std::shared_ptr<AsyncRetry>>::iterator it;
        };

        asio::coroutine coroutine; //onEvent's coroutine state

        void retry(const asio::error_code &reason);

        void handleResolve(const asio::error_code &error, tcp::resolver::iterator it);

        void handleConnect(const asio::error_code &error, tcp::resolver::iterator it);

        void handleEvent(const asio::error_code &error, std::string line);

        void reportAsioError(const asio::error_code &error);
    };

}