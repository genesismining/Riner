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
    struct Connection : public IOConnection, public std::enable_shared_from_this<Connection<SocketT>> {
        IOOnDisconnectedFunc &_onDisconnected;
        BaseIO::OnReceiveValueFunc &_onRecv;
        SocketT _socket;
        asio::streambuf _incoming;

        Connection(decltype(_onDisconnected) &onDisconnected, decltype(_onRecv) &onRecv, SocketT socket)
        : _onDisconnected(onDisconnected), _onRecv(onRecv), _socket(std::move(socket)) {
        };

        ~Connection() override {
            LOG(DEBUG) << "closing connection since no read/write operations are queued on it";
            MI_EXPECTS(_onDisconnected);
            _onDisconnected();
        }

        void asyncWrite(std::string outgoing) override {
            //capturing shared = shared_from_this() is critical here, it means that as long as the handler is not invoked
            //the refcount of this connection is kept above zero, and thus the connection is kept alive.
            //as soon as no async read or write on a connection is queued, the connection will close itself
            asio::async_write(_socket, asio::buffer(outgoing), [outgoing, shared = this->shared_from_this()] (const asio::error_code &error, size_t numBytes) {
                if (error) {
                    LOG(INFO) << "async write error #" << error <<
                              " in TcpLineConnection while trying to send '" << outgoing << "'";
                }
            });
        }

        void asyncRead() override {
            //capturing shared = shared_from_this() is critical here, it means that as long as the handler is not invoked
            //the refcount of this connection is kept above zero, and thus the connection is kept alive.
            //as soon as no async read or write on a connection is queued, the connection will close itself
            asio::async_read_until(_socket, _incoming, '\n', [this, shared = this->shared_from_this()]
                    (const asio::error_code &error, size_t numBytes) {

                if (error == asio::error::eof) {
                    LOG(INFO) << "BaseIO connection closed by other side";
                    return;
                }

                std::string line = "asio read_until error occured";

                if (!error && numBytes != 0) {
                    std::istream stream(&_incoming);
                    std::getline(stream, line);
                }

                MI_EXPECTS(line.size() == strlen(line.c_str()));

                MI_EXPECTS(_onRecv);
                _onRecv(shared //becomes weak_ptr<IOConnection> aka CxnHandle
                        , line);
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
    : _mode(mode)
    , _ioService()
    , _socket(_ioService) {
        startIoThread();
    }

    void BaseIO::writeAsync(CxnHandle handle, value_type outgoing) {
        if (auto cxn = handle.lock()) {
            cxn->asyncWrite(std::move(outgoing));
        }
        else {
            LOG(INFO) << "called writeAsync on connection that was closed";
        }
    }

    void BaseIO::readAsync(CxnHandle handle) {
        if (auto cxn = handle.lock()) {
            cxn->asyncRead();
        }
        else {
            LOG(INFO) << "called readAsync on connection that was closed";
        }
    }

    void BaseIO::startIoThread() {
        MI_EXPECTS(_thread == nullptr); //don't call launch twice on the same object!
        if (_thread) {
            LOG(ERROR) << "BaseIO::launchIoThread called after ioService thread was already started by another call. Ignoring this call.";
            return;
        }

        //start io service thread which will handle the async calls
        _thread = std::make_unique<std::thread>([this] () {
            setIoThread(); //set this thread id to be the IO Thread
            try {
                while(true) {
                    _ioService.run();

                    if (_shutdown) {
                        break;
                    } else {
                        _ioService.restart();
                    }
                    std::this_thread::sleep_for(std::chrono::seconds(1));
                    //returns whenever stopped or no handlers left to execute
                }
            }
            catch (std::exception &e) {
                LOG(ERROR) << "uncaught exception in BaseIO thread: " << e.what();
            }

            _ioThreadId = {}; //reset io thread id
        });
    }

    //used by client and server
    void BaseIO::createCxnWithSocket() {
        auto cxn = make_shared<Connection<TcpSocket>>(_onDisconnected, _onRecv, std::move(_socket));
        _socket = tcp::socket{_ioService}; //prepare socket for next connection
        MI_EXPECTS(_onConnected);
        _onConnected(cxn); //user is expected to use cxn here with other calls like readAsync(cxn)
    } //cxn refcount decreased and maybe destroyed if cxn was not used in _onConnected(cxn)

    //functions used by server
    void BaseIO::launchServer(uint16_t port, IOOnConnectedFunc &&onCxn, IOOnDisconnectedFunc &&onDc) {
        MI_EXPECTS(_thread && !hasLaunched());
        _hasLaunched = true;

        _onConnected    = std::move(onCxn);
        _onDisconnected = std::move(onDc);
        _acceptor = make_unique<tcp::acceptor>(_ioService, tcp::endpoint{tcp::v4(), port});

        serverListen();
    }

    void BaseIO::serverListen() {
        MI_EXPECTS(_acceptor);
        _acceptor->async_accept(_socket, [this] (const asio::error_code &error) { //socket operation
            if (!error) {
                createCxnWithSocket();
            }
            serverListen();
        });
    }

    //functions used by client
    void BaseIO::launchClient(std::string host, uint16_t port, IOOnConnectedFunc &&onCxn, IOOnDisconnectedFunc &&onDc) {
        MI_EXPECTS(_thread && !hasLaunched());
        _hasLaunched = true;

        _onConnected    = std::move(onCxn);
        _onDisconnected = [&, onDc = std::move(onDc)] () {
            _hasLaunched = false;
            onDc();
        };
        MI_ENSURES(_onDisconnected);
        MI_ENSURES(_onConnected);

        _resolver = make_unique<tcp::resolver>(_ioService);
        //socket is already prepared by ctor

        tcp::resolver::query query{host, std::to_string(port)};



        _resolver->async_resolve(query, [this] (auto &error, auto it) {
            if (!error) {
                auto endpoint = *it;
                _socket.async_connect(endpoint, [this, it] (auto &error) { //socket operation
                    clientIterateEndpoints(error, it);
                });
            }
            else {
                LOG(ERROR) << "asio could not async resolve query. Error #" << error.value() << ", " << error.message();
                _onDisconnected();
            }
        });
    }

    void BaseIO::clientIterateEndpoints(const asio::error_code &error, tcp::resolver::iterator it) {
        MI_EXPECTS(_resolver);
        if (!error) {
            LOG(INFO) << "successfully connected to " << it->host_name() << ':' << it->service_name();
            createCxnWithSocket();
        }
        else if (it != tcp::resolver::iterator()) {//default constructed iterator is "end"
            //The connection failed, but there's another endpoint to try.
            _socket.close(); //socket operation

            tcp::endpoint endpoint = *it;
            LOG(INFO) << "asio error while trying this endpoint: " << error.value() << " , " << error.message();
            LOG(INFO) << "connecting: trying different endpoint with ip: " << endpoint.address().to_string();
            _socket.async_connect(endpoint, [this, next = ++it] (const auto &error) { //socket operation
                clientIterateEndpoints(error, next);
            });
        } else {
            LOG(INFO) << "asio async connect: Error #" << error;
            _onDisconnected();
        }
    }

    void BaseIO::launchClientAutoReconnect(std::string host, uint16_t port, IOOnConnectedFunc &&onConnected, IOOnDisconnectedFunc &&onDisconnected) {
        MI_EXPECTS(_thread && !hasLaunched());
        auto failedTriesToConnect = make_shared<int>(0);

        _onConnected = [onConnected = std::move(onConnected), failedTriesToConnect] (CxnHandle cxn) {
            *failedTriesToConnect = 0;
            onConnected(std::move(cxn));
        };

        auto connect = [this, host, port, failedTriesToConnect] () {

            //if connecting has failed last time, wait a little bit before trying again
            if (*failedTriesToConnect > 0) {
                int waitMs = std::max(*failedTriesToConnect, 20) * 500;
                std::this_thread::sleep_for(std::chrono::milliseconds(waitMs));
            }
            ++*failedTriesToConnect;

            auto onCxn = std::move(_onConnected); //create temporaries to move from
            auto onDc = std::move(_onDisconnected);
            launchClient(host, port, std::move(onCxn), std::move(onDc));
        };

        _onDisconnected = [&, connect, onDisconnected = std::move(onDisconnected)] () {
            onDisconnected(); //call user defined callback first
            connect(); //then attempt reconnect
        };

        MI_ENSURES(_onDisconnected);
        MI_ENSURES(_onConnected);
        connect(); //make initial connection
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
        return _hasLaunched;
    }

    void BaseIO::setIoThread() {
        _ioThreadId = std::this_thread::get_id();
    }

    bool BaseIO::isIoThread() const {
        return std::this_thread::get_id() == _ioThreadId;
    }

}