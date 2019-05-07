//
//

#include <src/common/Assert.h>
#include "BaseIO.h"

namespace miner {

    using asio::ip::tcp;

    using TcpSocket = asio::ip::tcp::socket;

    template<class SocketT = TcpSocket>
    struct Connection : public IOConnection {
        BaseIO::OnReceiveValueFunc &onRecv;
        SocketT socket;
        asio::streambuf incoming;

        Connection(decltype(onRecv) &onRecv, asio::io_service &ioService)
        : onRecv(onRecv), socket(ioService) {
        };

        void asyncWrite(std::string outgoing) override {
            asio::async_write(socket, asio::buffer(outgoing), [outgoing] (const asio::error_code &error, size_t numBytes) {
                if (error) {
                    LOG(INFO) << "async write error #" << error <<
                              " in TcpLineConnection while trying to send '" << outgoing << "'";
                }
            });
        }

        void listen(std::shared_ptr<Connection> sharedThis) {

            LOG(INFO) << "Listening...";

            asio::async_read_until(socket, incoming, '\n', [this, sharedThis = std::move(sharedThis)]
                    (const asio::error_code &error, size_t numBytes) {

                if (error == asio::error::eof) {
                    LOG(INFO) << "BaseIO connection closed by other side";
                    return;
                }

                std::string line = "asio error occured"; //write something the user will notice in case they forget to check the asio::error

                if (!error && numBytes != 0) {
                    std::istream stream(&incoming);
                    std::getline(stream, line);
                }

                MI_EXPECTS(line.size() == strlen(line.c_str()));

                MI_EXPECTS(onRecv);
                onRecv(line, *this);

                listen(sharedThis);
            });
        }

        void accept(std::shared_ptr<Connection> sharedThis, tcp::acceptor &acceptor) {
            acceptor.async_accept(socket, [sharedThis] (const asio::error_code &error) {
                if (!error) {
                    LOG(INFO) << "BaseIO Connected!";
                }
                sharedThis->listen(sharedThis); //TODO: check if this is an infinite recursion, verify whether asio calls this handler with an error if the ioservice is about to be destroyed
            });
        }
    };

    BaseIO::~BaseIO() {
        _shutdown = true;

        if (_thread && _thread->joinable()) {
            _thread->join();
        }
    }

    BaseIO::BaseIO(IOMode mode, BaseIO::OnReceiveValueFunc &&onRecv)
    : _mode(mode), _onRecv(std::move(onRecv)) {
    }

    void BaseIO::asyncWrite(value_type outgoing, IOConnection &conn) {
        conn.asyncWrite(std::move(outgoing));
    }

    void BaseIO::launchServer(uint16_t port) {
        MI_EXPECTS(_thread == nullptr); //don't call launch twice on the same object!
        if (_thread) {
            LOG(ERROR) << "BaseIO::launchServer called after ioService thread was already started by another call. Ignoring this call.";
            return;
        }

        //start io service thread which will handle the async calls
        _thread = std::make_unique<std::thread>([this] () {
            try {
                while(true) {
                    _ioService.run();

                    if (_shutdown) {
                        break;
                    }
                    else {
                        _ioService.restart();
                    }
                    std::this_thread::sleep_for(std::chrono::seconds(1));
                    //returns whenever stopped or no handlers left to execute
                }
            }
            catch (std::exception &e) {
                LOG(ERROR) << "uncaught exception in BaseIO thread: " << e.what();
            }
        });

        auto conn = make_shared<Connection<TcpSocket>>(_onRecv, _ioService);
        _acceptor = make_unique<tcp::acceptor>(_ioService);

        conn->accept(conn, *_acceptor);
    }

    void BaseIO::launchClient(std::string host, uint16_t port) {
        if (_thread) {
            LOG(ERROR) << "BaseIO::launchClient called after ioService thread was already started by another call. Ignoring this call.";
            return;
        }
    }

}