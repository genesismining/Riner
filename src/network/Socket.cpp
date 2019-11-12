//
//

#ifdef HAS_OPENSSL
    #include <lib/asio/asio/include/asio/ssl/rfc2818_verification.hpp>
#endif

#include "Socket.h"

namespace miner {
    using namespace asio;

    Socket::Socket(asio::io_service &ioService, bool isClient)
    : _var(TcpSocket{ioService})
    , _isClient(isClient) {
    }

    Socket::Socket(asio::io_service &ioService, bool isClient, const SslDesc &desc)
    : _isClient(isClient) {
#ifdef HAS_OPENSSL
        using c = ssl::context;
        error_code err;

        c ctx {c::tls};

        uint64_t options = 0ULL;

        std::string server_client_str = isClient ? "client: " : "server: ";

        if (_isClient) {

            ctx.set_verify_mode(ssl::verify_peer);

            if (desc.client) {
                if (desc.client->certFile) {
                    auto file = desc.client->certFile.value();
                    ctx.load_verify_file(file, err); //assuming this can either be a CA file or a certificate
                    if (err) {
                        LOG(ERROR) << server_client_str << "error adding following verify file to tls socket: '" << file
                                   << "' asio error message: " << err.message();
                        return; //abort
                    }
                }
                else {
                    ctx.set_default_verify_paths(err);
                    if (err) {
                        LOG(INFO) << server_client_str << "error setting default verify paths when creating TLS socket: " << err.message();
                        return; //abort
                    }
                }
            }
            else {
                LOG(WARNING) << server_client_str << "SslDesc provided to BaseIO does not contain client related info. Aborting.";
            }
        }
        else { //server

            if (desc.server) {
                auto &info = desc.server.value();

                MI_EXPECTS(desc.server->onGetPassword); //expect password callback to exist
                auto onGetPassword = desc.server->onGetPassword;

                ctx.set_password_callback([onGetPassword = move(onGetPassword)] (size_t max_length, c::password_purpose purp) {
                    std::string pw = onGetPassword();
                    return pw.substr(0, max_length);
                });

                ctx.use_certificate_chain_file(info.certificateChainFile, err);
                if (err) {
                    LOG(ERROR) << server_client_str << "error during use_certificate_chain_file: "
                               << "asio error message: " << err.message();
                }

                ctx.use_private_key_file(info.privateKeyFile, c::pem, err);
                if (err) {
                    LOG(ERROR) << server_client_str << "error during use_private_key_file:"
                               << "asio error message: " << err.message();
                }

                if (!info.tmpDhFile.empty()) {
                    ctx.use_tmp_dh_file(info.tmpDhFile, err);
                    options |= (uint64_t)c::single_dh_use;
                    if (err) {
                        LOG(ERROR) << server_client_str<< "error during use_tmp_dh_file: "
                                   << "asio error message: " << err.message();
                    }
                }
            }
            else {
                LOG(WARNING) << server_client_str << "SslDesc provided to BaseIO does not contain server related info. Aborting.";
                return;
            }

        }
        options |=(uint64_t)c::default_workarounds
                | (uint64_t)c::no_sslv2
                | (uint64_t)c::no_sslv3
                | (uint64_t)c::no_tlsv1;

        ctx.set_options(options, err);

        if (err) {
            LOG(ERROR) << server_client_str << "error setting options for tls socket ctx. asio error message: " << err.message();
        }

        auto &sock = _var.emplace<SslTcpSocket>(ioService, ctx);
        (void)sock;
        //sock.lowest_layer().set_option(tcp::no_delay(true)); //leads to error apparently
#endif
    }

    void Socket::asyncHandshakeOrNoop(HandshakeFunc handler, optional<std::string> host) {
#ifdef HAS_OPENSSL
        visit<SslTcpSocket>(_var, [&] (SslTcpSocket &sock) {
            // Perform SSL handshake and verify the remote host's
            // certificate.
            if (_isClient) {
                //sock.set_verify_callback(ssl::rfc2818_verification(host.value()));

                sock.set_verify_callback([host] (bool preverified, ssl::verify_context &ctx) -> bool {
                    if (!host) {
                        LOG(ERROR) << "no host passed to client's cert verify callback";
                        return false;
                    }

                    char subject_name[256];
                    X509* cert = X509_STORE_CTX_get_current_cert(ctx.native_handle());
                    X509_NAME_oneline(X509_get_subject_name(cert), subject_name, 256);

                    LOG(INFO) << "verifying cert from: " << subject_name;
                    bool success = ssl::rfc2818_verification(host.value())(preverified, ctx);
                    if (success)
                        LOG(INFO) << "successfully verified";
                    else
                        LOG(ERROR) << "failed verification";

                    return success;
                });

            }
            sock.async_handshake(_isClient ? SslTcpSocket::client : SslTcpSocket::server, move(handler));
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
        (void)success;

#ifdef HAS_OPENSSL
        if (!success)
            visit<SslTcpSocket>(_var, [&] (SslTcpSocket &s) {
                result = &s.next_layer();
            });
#endif

        MI_EXPECTS(result);
        return *result;
    }

    bool Socket::isClient() const {
        return _isClient;
    }

}