//
//
#pragma once

#include <string>
#include <asio.hpp>
#include <src/util/Logging.h>
#include <asio/ip/tcp.hpp>
#include <src/common/Pointers.h>
#include <src/network/IOTypeLayer.h>
#include <src/util/Copy.h>
#include <src/util/LockUtils.h>
#include <list>

namespace miner {

    class IOConnection {
        friend class BaseIO;

        //this function is not exposed to prevent confusion with the type layers
        virtual void asyncWrite(std::string outgoingStr) = 0;
        virtual void asyncRead() = 0;
    public:
        virtual ~IOConnection() = default;
    };

    class BaseIO {
    public:
        using value_type = std::string;
        using OnReceiveValueFunc = IOOnReceiveValueFunc<value_type>;

        explicit BaseIO(IOMode mode);
        ~BaseIO();

        DELETE_COPY_AND_MOVE(BaseIO); //because Connection<T> is referencing BaseIO::_onRecv

        //launch as server or client, once launched cannot be changed for an instance
        void launchServer(uint16_t listenOnPort, IOOnConnectedFunc &&, IOOnDisconnectedFunc && = ioOnDisconnectedNoop);
        void launchClient(std::string host, uint16_t port, IOOnConnectedFunc &&, IOOnDisconnectedFunc && = ioOnDisconnectedNoop);
        void launchClientAutoReconnect(std::string host, uint16_t port, IOOnConnectedFunc &&, IOOnDisconnectedFunc && = ioOnDisconnectedNoop);

        void writeAsync(CxnHandle, value_type outgoing);

        void setOnReceive(OnReceiveValueFunc &&);

        bool hasLaunched() const; //can be called from any thread (thread safe)

        bool isIoThread() const;

        template<class Fn>
        void postAsync(Fn &&func) {
            asio::post(_ioService, std::forward<Fn>(func));
        }

        void retryAsyncEvery(milliseconds interval, std::function<bool()> &&pred);

    private:
        void createCxnWithSocket();
        void serverListen();
        void clientIterateEndpoints(const asio::error_code &error, asio::ip::tcp::resolver::iterator it);

        IOMode _mode;

        using tcp = asio::ip::tcp;

        void startIoThread();
        decltype(std::this_thread::get_id()) _ioThreadId;
        void setIoThread();

        struct AsyncRetry;
        LockGuarded<std::list<shared_ptr<AsyncRetry>>> _activeRetries;

        using WaitHandler = std::function<void(const asio::error_code &)>;

        struct AsyncRetry { //used to store state for "retryAsyncEvery(...)" function
            asio::steady_timer timer;
            std::function<bool()> pred;
            WaitHandler waitHandler;
            std::list<std::shared_ptr<AsyncRetry>>::iterator it;
        };

        asio::io_service _ioService;
        tcp::socket _socket; //depends on _ioService. gets moved into new connections as they are constructed
        unique_ptr<tcp::acceptor> _acceptor; //for server
        unique_ptr<tcp::resolver> _resolver; //for client

        IOOnConnectedFunc _onConnected = ioOnConnectedNoop;
        IOOnDisconnectedFunc _onDisconnected = ioOnDisconnectedNoop;

        OnReceiveValueFunc _onRecv = ioOnReceiveValueNoop<value_type>; //_onRecv referenced by instances of Connection<T>;
        std::atomic_bool _shutdown {false};
        std::atomic_bool _hasLaunched {false};

        unique_ptr<std::thread> _thread;

    };

}