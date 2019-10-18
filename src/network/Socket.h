//
//

#pragma once

#include <asio.hpp>
#include <asio/ip/tcp.hpp>
#include <src/common/Pointers.h>
#include <src/common/Variant.h>
#include <src/network/SslDesc.h>
#include <src/util/Copy.h>
#include <src/common/Assert.h>

#ifdef HAS_OPENSSL
#include <lib/asio/asio/include/asio/ssl/stream.hpp>
#endif

namespace miner {

    using asio::ip::tcp;

    class Socket {
        using TcpSocket = tcp::socket;

#ifdef HAS_OPENSSL
        using SslTcpSocket = asio::ssl::stream<tcp::socket>;
#else
        class None {None() = default;};
        using SslTcpSocket = None;
#endif
        variant<nullptr_t, TcpSocket, SslTcpSocket> _var {nullptr}; //TODO: replace with old school object oriented virtual stuff!
        bool _isClient = false;

    public:
        explicit Socket(asio::io_service &, bool isClient); //constructs a regular tcp socket
        Socket(asio::io_service &, bool isClient, const SslDesc &); //constructs a ssl enabled tcp socket or does nothing if OpenSSL is not available
        Socket() = default;

        operator bool() const {
            return mpark::get_if<nullptr_t>(&_var);
        }

        bool isClient() const;

        /**
         * get a reference to the underlying socket type.
         * @return common base class of tcp and tcp ssl socket types
         */
        asio::basic_stream_socket<tcp> &get();

        /**
         * if initialized as a ssl socket, performs ssl handshake, otherwise nothing. use handler to continue
         */
        using HandshakeFunc = std::function<void(const asio::error_code &)>;

        void asyncHandshakeOrNoop(HandshakeFunc handler, optional<std::string> host = {});
    };

}