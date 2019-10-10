//
//

#ifdef HAS_OPENSSL
    #include <lib/asio/asio/include/asio/ssl/rfc2818_verification.hpp>
#endif

#include "Socket.h"

namespace miner {
    using namespace asio;

    Socket::Socket(asio::io_service &ioService)
    : _var(TcpSocket{ioService}) {
    }

    Socket::Socket(asio::io_service &ioService, const SslDesc &desc) {
#ifdef HAS_OPENSSL
        using c = ssl::context;

        c ctx {c::tls};

        ctx.set_options((uint64_t)c::default_workarounds |
                        (uint64_t)c::no_sslv2 |
                        (uint64_t)c::no_sslv3 |
                        (uint64_t)c::no_tlsv1
        );

        error_code err;

        if (desc.certFiles.empty()) {
            ctx.set_default_verify_paths(err);
            if (err) {
                LOG(INFO) << "error setting default verify paths when creating TLS socket: " << err.message();
                return; //abort
            }
        }
        else {
            //add custom verify paths
            for (auto &path : desc.certFiles) {
                ctx.add_verify_path(path, err);
                if (err) {
                    LOG(ERROR) << "error adding following verify path to tls socket: '" << path << "' asio error message: " << err.message();
                }
            }
        }

        auto &sock = _var.emplace<SslTcpSocket>(ioService, ctx);

        sock.lowest_layer().set_option(tcp::no_delay(true));
#endif
    }

    void Socket::asyncHandshakeOrNoop(bool isClient, const std::string &host, HandshakeFunc &&handler) {
#ifdef HAS_OPENSSL
        visit<SslTcpSocket>(_var, [&] (SslTcpSocket &sock) {
            // Perform SSL handshake and verify the remote host's
            // certificate.
            sock.set_verify_mode(ssl::verify_peer);
            sock.set_verify_callback(ssl::rfc2818_verification(host));
            sock.async_handshake(isClient ? SslTcpSocket::client : SslTcpSocket::server, move(handler));
        });
#else
        handler({}); //just call handler synchronously, so no additional codepath is necessary at callee
#endif
    }

    asio::basic_stream_socket<tcp> &Socket::get() {
        asio::basic_stream_socket<tcp> *result = nullptr;

        bool success = visit<TcpSocket>(_var, [&] (TcpSocket &s) {
            result = &s;
        });

#ifdef HAS_OPENSSL
        if (!success)
            visit<SslTcpSocket>(_var, [&] (SslTcpSocket &s) {
                result = &s.next_layer();
            });
#endif

        MI_EXPECTS(result);
        return *result;
    }

}