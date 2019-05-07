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

namespace miner {

    class IOConnection {
        friend class BaseIO;

        //this function is not exposed to prevent confusion with the type layers
        virtual void asyncWrite(std::string outgoingStr) = 0;

    public:
        virtual ~IOConnection() = default;
    };

    class BaseIO {
    public:
        using value_type = std::string;
        using OnReceiveValueFunc = IOOnReceiveValueFunc<value_type>;

        BaseIO(IOMode mode, OnReceiveValueFunc &&onRecv);
        ~BaseIO();

        DELETE_COPY_AND_MOVE(BaseIO); //because Connection<T> is referencing BaseIO::_onRecv

        //launch as server or client, once launched cannot be changed for an instance
        void launchServer(uint16_t listenOnPort);
        void launchClient(std::string host, uint16_t port);

        void asyncWrite(value_type outgoing, IOConnection &);

        template<class Fn>
        void postAsync(Fn &&func) {
            asio::post(_ioService, std::forward<Fn>(func));
        }

    private:
        IOMode _mode;

        using tcp = asio::ip::tcp;

        asio::io_service _ioService;
        unique_ptr<tcp::acceptor> _acceptor;
        OnReceiveValueFunc _onRecv; //referenced by instances of Connection<T>;
        std::atomic_bool _shutdown {false};
        unique_ptr<std::thread> _thread;

    };

}