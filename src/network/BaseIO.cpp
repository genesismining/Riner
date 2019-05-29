//
//

#include <src/common/Assert.h>
#include "BaseIO.h"

//#include <asio/ssl.hpp>

namespace miner {

    using asio::ip::tcp;
    using TcpSocket = tcp::socket;

    //namespace ssl = asio::ssl;
    //using TcpSslSocket = ssl::stream<tcp::socket>;

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

        void listen(shared_ptr<Connection> sharedThis) {

            LOG(INFO) << "Listening...";

            asio::async_read_until(socket, incoming, '\n', [this, sharedThis = std::move(sharedThis)]
                    (const asio::error_code &error, size_t numBytes) {

                if (error == asio::error::eof) {
                    LOG(INFO) << "BaseIO connection closed by other side";
                    return;
                }

                std::string line = "asio error occured";

                if (!error && numBytes != 0) {
                    std::istream stream(&incoming);
                    std::getline(stream, line);
                }

                MI_EXPECTS(line.size() == strlen(line.c_str()));

                MI_EXPECTS(onRecv);
                onRecv(sharedThis //becomes weak_ptr<IOConnection> aka CxnHandle
                        , line);

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

    BaseIO::BaseIO(IOMode mode)
    : _mode(mode) {
    }

    void BaseIO::writeAsync(CxnHandle handle, value_type outgoing) {
        if (auto cxn = handle.lock()) {
            cxn->asyncWrite(std::move(outgoing));
        }
    }

    void BaseIO::launchServer(uint16_t port, IOOnConnectedFunc &&onConnect) {
        MI_EXPECTS(_thread == nullptr); //don't call launch twice on the same object!
        if (_thread) {
            LOG(ERROR) << "BaseIO::launchServer called after ioService thread was already started by another call. Ignoring this call.";
            return;
        }

        _onConnect = std::move(onConnect);

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
        _acceptor = make_unique<tcp::acceptor>(_ioService, tcp::endpoint{tcp::v4(), port});
        conn->accept(conn, *_acceptor);
    }

    void BaseIO::launchClient(std::string host, uint16_t port, IOOnConnectedFunc &&onConnect) {
        MI_EXPECTS(_thread == nullptr); //don't call launch twice on the same object!
        if (_thread) {
            LOG(ERROR) << "BaseIO::launchClient called after ioService thread was already started by another call. Ignoring this call.";
            return;
        }

        _onConnect = std::move(onConnect);
    }

    void BaseIO::retryAsyncEvery(milliseconds interval, std::function<bool()> &&pred) {

        auto shared = std::shared_ptr<AsyncRetry>(new AsyncRetry {
                {_ioService, interval}, std::move(pred), {}, {}
        });

        auto weak = make_weak(shared);

        {//push AsyncRetry object to _activeRetries
            auto list = _activeRetries.lock();
            list->push_front(shared);
            shared->it = list->begin();
        }

        //define function that re-submits asio::steady_timer with this function
        shared->waitHandler = [this, weak, interval] (const asio::error_code &error) {

            if (error && error != asio::error::operation_aborted) {
                LOG(INFO) << "wait handler error " << error;
                return;
            }

            if (auto shared = weak.lock()) {

                //resubmit this wait handler if predicate is false, otherwise clean up AsyncRetry state
                if (shared->pred()) {
                    _activeRetries.lock()->erase(shared->it); //'shared' now has the last ref

                    //'shared' is the last reference to this function, do not let this
                    //function continue in a destroyed state
                    return;
                }
                else {
                    shared->timer = asio::steady_timer{_ioService, interval};
                    shared->timer.async_wait(shared->waitHandler);
                }

            }

        };

        //initiate the periodic waitHandler calls with empty error_code
        postAsync([shared] {
            shared->waitHandler(asio::error_code());
        });
    }

    void BaseIO::setOnReceive(OnReceiveValueFunc &&onRecv) {
        _onRecv = onRecv;
    }

    bool BaseIO::hasLaunched() const {
        return (bool)_thread;
    }

    void BaseIO::setIoThread() {
        _ioThreadId = std::this_thread::get_id();
    }

    bool BaseIO::isIoThread() const {
        return std::this_thread::get_id() == _ioThreadId;
    }

}