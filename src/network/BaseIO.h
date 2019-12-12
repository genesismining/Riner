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
#include "Socket.h"

namespace miner {

    class IOConnection {
        friend class BaseIO;
        friend class CxnHandle;

        //this function is not exposed to prevent confusion with the type layers
        virtual void asyncWrite(std::string outgoingStr) = 0;
        virtual void asyncRead() = 0;

        //a connection is always associated with a BaseIO instance. This getter is used to catch the error where a
        //CxnHandle is used with a foreign BaseIO instance by accident
        virtual uint64_t getAssociatedBaseIOUid() = 0;
    public:
        virtual ~IOConnection() = default;
    };

    /**
     * `BaseIO` is the bottom most IOTypeLayer (that doesn't actually derive from IOTypeLayer, as it has no successor).
     * It implements the actual IO logic based on `std::string` lines (ending with a `'\n'` character)
     */
    class BaseIO {
        template<class U, class V>
        friend class IOTypeLayer;
    public:
        using value_type = std::string;
        using OnReceiveValueFunc = IOOnReceiveValueFunc<value_type>;

        /**
         * `BaseIO`'s constructor starts the IO thread but does not yet enqueue any handlers on it.
         * Connections initiated only in the `launchClient()` and `launchServer()` functions.
         * @param mode
         */
        explicit BaseIO(IOMode mode);
        ~BaseIO();

        DELETE_COPY_AND_MOVE(BaseIO); //because Connection<T> is referencing BaseIO::_onRecv

        void enableSsl(const SslDesc &);

        //launch as server or client, once launched cannot be changed for an instance
        void launchServer(uint16_t listenOnPort, IOOnConnectedFunc &&, IOOnDisconnectedFunc && = ioOnDisconnectedNoop);
        void launchClient(std::string host, uint16_t port, IOOnConnectedFunc &&, IOOnDisconnectedFunc && = ioOnDisconnectedNoop);

        //only reconnects if the connection was NOT closed due to
        // - this BaseIO object being destroyed
        // - disconnectAll() being called on this object //if you want to have reconnect functionality in this case, call launchClientAutoReconnect again after disconnectAll()
        void launchClientAutoReconnect(std::string host, uint16_t port, IOOnConnectedFunc &&, IOOnDisconnectedFunc && = ioOnDisconnectedNoop);

        void writeAsync(CxnHandle, value_type outgoing);
        void readAsync(CxnHandle);

        void setOnReceive(OnReceiveValueFunc &&);

        bool hasLaunched() const; //can be called from any thread (thread safe)

        bool isIoThread() const;

        template<class Fn>
        void postAsync(Fn &&func) {
            asio::post(*_ioService, std::forward<Fn>(func));
        }
        void retryAsyncEvery(milliseconds interval, std::function<bool()> &&pred, std::function<void()> &&onCancelled);

        uint64_t getUid();

        void disconnectAll();
    protected:
        void stopIOThread(); //blocking, joins the io thread, no parallel handler execution is happening after this function returns
        bool ioThreadRunning() const; //not thread safe

    private:
        void createCxnWithSocket(unique_ptr<Socket>);
        void serverListen();
        void clientIterateEndpoints(const asio::error_code &error, asio::ip::tcp::resolver::iterator it);
        void handshakeHandler(const asio::error_code &error, unique_ptr<Socket>); //takes ownership of socket already

        IOMode _mode;

        static uint64_t generateUid();

        uint64_t _uid = generateUid(); //uniquely identifies this BaseIO instance, so that the error can be caught where
        // CxnHandles from another BaseIO instance are used with this BaseIO instance by mistake.

        using tcp = asio::ip::tcp;

        void startIOThread();
        decltype(std::this_thread::get_id()) _ioThreadId;
        void setIoThreadId();

        void abortAllAsyncRetries();

        void resetIoService();

        struct AsyncRetry;
        LockGuarded<std::list<shared_ptr<AsyncRetry>>> _activeRetries;

        using WaitHandler = std::function<void(const asio::error_code &)>;

        struct AsyncRetry { //used to store state for "retryAsyncEvery(...)" function
            asio::steady_timer timer;
            std::function<bool()> pred;
            std::function<void()> onCancelled;
            WaitHandler waitHandler;
        };

        IOOnConnectedFunc _onConnected = ioOnConnectedNoop;
        IOOnDisconnectedFunc _onDisconnected = ioOnDisconnectedNoop;

        //TODO: refactor the BaseIO object to be just a wrapper around a unique_ptr that owns the actual BaseIO object (and while you're at it, make it 2 different ones for server and client). that way it can all be restarted in a RAII fashion without crazy dependencies like expressed below
        unique_ptr<asio::io_service> _ioService; //IMPORTANT: sadly, *_ioService refs are stored in the asio::steady_timers inside _activeRetries. abortAllAsyncRetries() must be called before resetting this unique_ptr.
        // _ioService must be declared after _onDisconnected since stop() may call _onDisconnected().

        //socket depends on _ioService. gets moved into new connections as they are constructed
        //can be a plain old tcp::socket or a ssl stream around a tcp::socket
        optional<SslDesc> _sslDesc;
        unique_ptr<Socket> _socket;

        unique_ptr<tcp::acceptor> _acceptor; //for server
        unique_ptr<tcp::resolver> _resolver; //for client


        OnReceiveValueFunc _onRecv = ioOnReceiveValueNoop<value_type>; //_onRecv referenced by instances of Connection<T>;
        std::atomic_bool _shutdown {false};
        std::atomic_bool _hasLaunched {false};

        unique_ptr<std::thread> _thread;

        void prepareSocket(bool isClient);

        bool sslEnabledButNotSupported();
    };

}