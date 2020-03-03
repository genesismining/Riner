//
//

#include <src/common/Assert.h>
#include <src/util/AsioErrorUtil.h>
#include "BaseIO.h"

#ifdef HAS_OPENSSL
#include <asio/ssl.hpp>
#endif

namespace riner {

    using asio::ip::tcp;

    //TODO: remove IOConnection base class. The Socket abstraction now does what was originally thought to be the responsibility of this
    //class hierarchy.
    struct Connection : public IOConnection, public std::enable_shared_from_this<Connection> {
        IOOnDisconnectedFunc &_onDisconnected;
        BaseIO::OnReceiveValueFunc &_onRecv;
        unique_ptr<Socket> _socket;
        asio::streambuf _incoming;
        uint64_t _baseIOUid = 0;

        static size_t generateConnectionUid() {
            static std::atomic<uint64_t> nextUid = {1};
            return nextUid++;
        }

        uint64_t _connectionUid = generateConnectionUid();

        Connection(decltype(_onDisconnected) &onDisconnected, decltype(_onRecv) &onRecv, unique_ptr<Socket> socket, uint64_t baseIOUid)
        : _onDisconnected(onDisconnected), _onRecv(onRecv), _socket(std::move(socket)), _baseIOUid(baseIOUid) {
        };

        ~Connection() override {
            VLOG(0) << "closing connection #" << _connectionUid << " (likely because no read/write operations are queued on it)";
            RNR_EXPECTS(_onDisconnected);
            _onDisconnected();
        }

        uint64_t getAssociatedBaseIOUid() override {
            RNR_EXPECTS(_baseIOUid != 0); //baseIO Uids start at 1, if it's 0, it wasn't set
            return _baseIOUid;
        }

        uint64_t getConnectionUid() override {
            RNR_EXPECTS(_connectionUid != 0); //connection uids start at 1. this one was not initiailzed!
            return _connectionUid;
        }

        void asyncWrite(std::string outgoing) override {
            //capturing shared = shared_from_this() is critical here, it means that as long as the handler is not invoked
            //the refcount of this connection is kept above zero, and thus the connection is kept alive.
            //as soon as no async read or write action is queued on a given connection is, the connection will close itself
            _socket->async_write(asio::buffer(outgoing), [outgoing, shared = this->shared_from_this()] (const asio::error_code &error, size_t numBytes) {
                if (error) {
                    LOG(WARNING) << "async write error " << asio_error_name_num(error) <<
                              " in Connection while trying to send '" << outgoing << "'";
                }
            });
        }

        void asyncRead() override {
            //capturing shared = shared_from_this() is critical here, it means that as long as the handler is not invoked
            //the refcount of this connection is kept above zero, and thus the connection is kept alive.
            //as soon as no async read or write action is queued on a given connection is, the connection will close itself
            VLOG(3) << "ReadUntil queued";
            _socket->async_read_until(_incoming, '\n', [this, shared = this->shared_from_this()]
                    (const asio::error_code &error, size_t numBytes) {

                VLOG(3) << "ReadUntil scheduled";
                if (error) {
                    if (error.value() == asio::error::eof) {
                        LOG(INFO) << "connection #" << _connectionUid << " closed from other side (eof)";
                    }
                    else {
                        LOG(WARNING) << "asio error " << asio_error_name_num(error) << " during async_read_until: "
                                     << error.message();
                    }
                    return;
                }

                std::string line;
                {
                    std::istream stream(&_incoming);
                    std::getline(stream, line);
                }

                RNR_EXPECTS(line.size() == strlen(line.c_str()));

                RNR_EXPECTS(_onRecv);
                _onRecv(CxnHandle{shared} //becomes weak_ptr<IOConnection> aka CxnHandle
                        , line);
            });
        }

    };

    BaseIO::~BaseIO() {
        RNR_EXPECTS(!isIoThread());
        stopIOThread();
        abortAllAsyncRetries();
        //handlers get destroyed in _ioService dtor
    }

    BaseIO::BaseIO(IOMode mode)
    : _mode(mode)
    , _ioService(make_unique<asio::io_service>()) {
        startIOThread();
    }

    void BaseIO::writeAsync(CxnHandle handle, value_type outgoing) {

        if (auto cxn = handle.lock(*this)) {
            cxn->asyncWrite(std::move(outgoing)); //cxn shared_ptr is captured inside this func's async handler (which prolongs it's lifetime)
        }
        else {
            VLOG(2) << "called writeAsync on connection that was closed";
        }
    }

    void BaseIO::readAsync(CxnHandle handle) {
        if (auto cxn = handle.lock(*this)) {
            cxn->asyncRead(); //cxn shared_ptr is captured inside this func's async handler (which prolongs it's lifetime)
        }
        else {
            VLOG(2) << "called readAsync on connection that was closed";
        }
    }

    uint64_t BaseIO::generateUid() {
        static std::atomic<uint64_t> nextUid = {1};
        return nextUid++;
    }

    void BaseIO::enableSsl(const SslDesc &desc) {
        _sslDesc = desc;
#ifndef HAS_OPENSSL
        LOG(WARNING) << "cannot enableSsl() on BaseIO since binary was compiled without OpenSSL support. Recompilation with OpenSSL required.";
#endif
    }

    bool BaseIO::sslEnabledButNotSupported() {
#ifdef HAS_OPENSSL
        return false;
#endif
        return _sslDesc.has_value();
    }

    void BaseIO::startIOThread() {
        RNR_EXPECTS(_thread == nullptr);
        if (_thread) {
            LOG(WARNING) << "BaseIO::launchIoThread called after ioService thread was already started by another call. Ignoring this call.";
            return;
        }
        _shutdown = false; //reset shutdown to false if it remained true e.g. due to disconnectAll();

        //start io service thread which will handle the async calls
        _thread = std::make_unique<std::thread>([this] () {
            SetThreadNameStream{} << "io#" << _uid; //thread name shows BaseIO's uid
            setIoThreadId(); //set this thread id to be the IO Thread

            try {

                std::vector<int> wait_durations_ms = {0, 1, 2, 5, 10, 50, 100, 150, 200, 400, 600, 1000, 2000};
                size_t current_wait = 0;

                while(true) {

                    auto handlers_done = _ioService->run();
                    //returns whenever stopped or no handlers left to execute

                    if (handlers_done > 0)
                        current_wait = 0; //reset wait durations if work was done!

                    if (_shutdown) {
                        break;
                    } else {
                        _ioService->restart();
                    }

                    //wait (happens e.g. if no handlers were queued as soon as the thread got launched)

                    current_wait = std::min(current_wait, wait_durations_ms.size()-1); //clamp to last
                    auto dur = wait_durations_ms.at(current_wait);
                    ++current_wait; //wait for longer next time

                    std::this_thread::sleep_for(std::chrono::milliseconds(dur));
                }
            }
            catch (std::exception &e) {
                LOG(ERROR) << "uncaught exception in BaseIO thread: " << e.what();
            }

            _ioThreadId = {}; //reset io thread id
        });
        RNR_ENSURES(_thread);
        VLOG(3) << "BaseIO::startIOThread";
    }

    void BaseIO::stopIOThread() { //TODO: atm subclasses are required to call stopIOThread from their dtor but implementors can forget to do so. (which causes crashes that are hard to pin down)
        // A proper way of implementing this would be to have a std::lock_guard style RAII object that guards the io thread's scope on user side.
        // e.g. launchClient returns an IOThreadGuard which has a dtor that allows the io thread to stop

        if (_thread) {
            _shutdown = true;

            _ioService->stop();

            RNR_EXPECTS(_thread->joinable());
            _thread->join();
            _thread = nullptr; //so that assert(!_thread) works as expected
            _hasLaunched = false;
            //_shutdown = false; //shutdown should NOT be set to false here, since the onDisconnect handler checks _shutdown to decide whether to autoReconnect. Instead it gets set to false upon iothread restart
        }
        RNR_ENSURES(!_thread);
    }

    //used by client and server
    void BaseIO::createCxnWithSocket(unique_ptr<Socket> sock) { //TODO: make method const?
        RNR_EXPECTS(sock);
        auto cxn = make_shared<Connection>(_onDisconnected, _onRecv, std::move(sock), _uid);
        RNR_EXPECTS(_onConnected);
        _onConnected(CxnHandle{cxn}); //user is expected to use cxn here with other calls like readAsync(cxn)
    } //cxn refcount decremented and maybe destroyed if cxn was not used in _onConnected(cxn)

    //functions used by server
    void BaseIO::launchServer(uint16_t port, IOOnConnectedFunc &&onCxn, IOOnDisconnectedFunc &&onDc) {
        if (sslEnabledButNotSupported())
            return;

        VLOG(2) << "BaseIO::launchServer: hasLaunched: " << hasLaunched() << " _thread: " << !!_thread;

        RNR_EXPECTS(_ioService);
        RNR_EXPECTS(_thread);
        RNR_EXPECTS(!hasLaunched());
        _hasLaunched = true;

        _onConnected    = std::move(onCxn);
        _onDisconnected = std::move(onDc);
        _acceptor = make_unique<tcp::acceptor>(*_ioService, tcp::endpoint{tcp::v4(), port});

        bool isClient = false;
        prepareSocket(isClient);
        serverListen();
    }

    void BaseIO::serverListen() {
        RNR_EXPECTS(_acceptor);
        RNR_EXPECTS(_socket);
        _acceptor->async_accept(_socket->tcpStream(), [this] (const asio::error_code &error) { //socket operation
            if (!error) {
                bool isClient = false;

                _socket->tcpStream().set_option(tcp::no_delay(true));
                auto suSock = make_shared<unique_ptr<Socket>>(move(_socket)); //move out current socket
                prepareSocket(isClient); //make new socket

                (*suSock)->asyncHandshakeOrNoop([this, suSock] (const asio::error_code &error) {
                    if (*suSock) {
                        handshakeHandler(error, move(*suSock));
                    }
                });
            }
            serverListen();
        });
    }

    void BaseIO::resetIoService() {
        abortAllAsyncRetries(); //retries are referencing _ioService, destroy them before resetting the _ioService
        _socket.reset();
        _acceptor.reset();
        _resolver.reset();
        _ioService = make_unique<asio::io_service>();
    }

    void BaseIO::prepareSocket(bool isClient) {
        RNR_EXPECTS(_ioService);
        if (_sslDesc) { //create ssl socket
            _socket = make_unique<Socket>(*_ioService, isClient, _sslDesc.value());
        }
        else { //create tcp socket
            _socket = make_unique<Socket>(*_ioService, isClient);
        }
    }

    //functions used by client
    void BaseIO::launchClient(std::string host, uint16_t port, IOOnConnectedFunc &&onCxn, IOOnDisconnectedFunc &&onDc) {
        if (sslEnabledButNotSupported())
            return;

        RNR_EXPECTS(_ioService);
        RNR_EXPECTS(_thread);
        RNR_EXPECTS(!hasLaunched());
        _hasLaunched = true;
        VLOG(3) << "BaseIO::launchClient: hasLaunched: " << hasLaunched() << " _thread: " << !!_thread;

        _onConnected    = std::move(onCxn);
        _onDisconnected = [&, onDc = std::move(onDc)] () {
            _hasLaunched = false;
            onDc();
        };
        RNR_ENSURES(_onDisconnected);
        RNR_ENSURES(_onConnected);

        _resolver = make_unique<tcp::resolver>(*_ioService);
        bool isClient = true;
        prepareSocket(isClient);

        tcp::resolver::query query{host, std::to_string(port)};

        _resolver->async_resolve(query, [this] (auto &error, auto it) {
            if (!error) {
                auto endpoint = *it;
                _socket->tcpStream().async_connect(endpoint, [this, it] (auto &error) { //socket operation
                    clientIterateEndpoints(error, it);
                });
            }
            else {
                LOG(ERROR) << "asio could not resolve query. Error #" << error.value() << ", " << error.message();
                _onDisconnected();
            }
        });
    }

    void BaseIO::clientIterateEndpoints(const asio::error_code &error, tcp::resolver::iterator it) {
        RNR_EXPECTS(_resolver);
        auto it_end = tcp::resolver::iterator(); //default constructed iterator is "end"
        if (!error) {
            LOG(INFO) << "successfully connected to '" << it->host_name() << ':' << it->service_name() << "'";

            //std::function that the upcoming lambda will be converted to demands copyability. so no unique_ptrs may be captured.
            _socket->tcpStream().set_option(tcp::no_delay(true));
            auto suSock = make_shared<unique_ptr<Socket>>(move(_socket));

            (*suSock)->asyncHandshakeOrNoop([this, suSock] (const asio::error_code &error) mutable {
                if (*suSock) {
                    handshakeHandler(error, std::move(*suSock));
                }
            }, it->host_name());
        }
        else if (it != it_end) {
            //The connection failed, but there's another endpoint to try.
            _socket->tcpStream().close(); //socket operation

            tcp::endpoint endpoint = *it;
            VLOG(1) << "when trying endpoint "<< it->host_name() << ":" << it->service_name() <<" - " << asio_error_name_num(error) << ": " << error.message();
            auto next = ++it;
            if (next != it_end) {
                VLOG(3) << "now trying different endpoint "<< next->host_name() << ":" << next->service_name();
            }
            _socket->tcpStream().async_connect(endpoint, [this, next = ++it] (const auto &error) { //socket operation
                clientIterateEndpoints(error, next);
            });
        }
        else {
            VLOG(3) << "async connect: " << asio_error_name_num(error) << ": " << error.message();
            _onDisconnected();
        }
    }

    //used by client and server
    //takes ownership of a socket, so that in the case of being called by the server, the server can already start preparing a new
    //socket and keep listening while the async handshake is not yet finished (this is not the call responsible for that, but the lambda encapsulating it)
    void BaseIO::handshakeHandler(const asio::error_code &error, unique_ptr<Socket> sock) { //TODO: make method const?
        if (!error) {
            if (_sslDesc) //if ssl is enabled a handshake happened successfully, otherwise nothing happened... successfully!
                VLOG(0) << "successfully performed handshake";
            createCxnWithSocket(std::move(sock));
        }
        else {
            if (error)
                VLOG(0) << "asio async " << (sock->isClient() ? "client" : "server") << " handshake: Error #" << error << ": " << error.message();
            _onDisconnected();
        }
    }

    void BaseIO::launchClientAutoReconnect(std::string host, uint16_t port, IOOnConnectedFunc &&onConnected, IOOnDisconnectedFunc &&onDisconnected) {
        RNR_EXPECTS(_thread && !hasLaunched());
        auto failedTriesToConnect = make_shared<int>(0);

        _onConnected = [onConnected = std::move(onConnected), failedTriesToConnect] (CxnHandle cxn) {
            *failedTriesToConnect = 0;
            onConnected(std::move(cxn));
        };

        auto connect = [this, host, port, failedTriesToConnect] () {
            if (this->_shutdown)
                return;

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

        RNR_ENSURES(_onDisconnected);
        RNR_ENSURES(_onConnected);
        connect(); //make initial connection
    }

    void BaseIO::disconnectAll() {
        VLOG(2) << "io#" << _uid << " disconnect if connected";
        RNR_EXPECTS(!isIoThread());

        stopIOThread();
        resetIoService();
        startIOThread();
    }

    void BaseIO::abortAllAsyncRetries() {

        //expect no handlers to be running right now!
        RNR_EXPECTS(!ioThreadRunning());
        //user has "onNeverResponded" handlers on callAsyncRetryNTimes which uses retryAsyncEvery,
        //the onCancelled handler must be called at a time where the user can be sure that the ioThread is no longer around
        //to interact with their resources. Therefore let's guarantee that the ioThread is joined here.

        {auto list = _activeRetries.lock();

            for (auto &retry : *list) {
                retry->onCancelled();
            }

            list->clear();
        }
    }

    void BaseIO::retryAsyncEvery(milliseconds interval, std::function<bool()> &&pred, std::function<void()> &&onCancelled) {

        auto shared = std::shared_ptr<AsyncRetry>(new AsyncRetry {
                {*_ioService, interval}, std::move(pred), std::move(onCancelled), {}
        });

        auto weak = make_weak(shared);

        {//push AsyncRetry object to _activeRetries
            auto list = _activeRetries.lock();
            list->push_front(shared);
        }

        //define function that re-submits asio::steady_timer with this function
        shared->waitHandler = [this, weak, interval] (const asio::error_code &error) {

            if (error == asio::error::operation_aborted) { //TODO: remove
                LOG(INFO) << "retry handler successfully aborted";
            }

            if (error && error != asio::error::operation_aborted) {
                LOG(INFO) << "wait handler error " << asio_error_name_num(error);
            }

            if (auto shared = weak.lock()) {

                if (error == asio::error::operation_aborted) {
                    shared->onCancelled();
                }

                //resubmit this wait handler if predicate is false, otherwise clean up AsyncRetry state
                if (error || shared->pred()) {
                    {
                        auto list = _activeRetries.lock();
                        auto it = std::find(list->begin(), list->end(), shared);
                            list->erase(it); //'shared' now has the last ref
                    }

                    //'shared' is the last reference to this function
                    //destroy it by returning from this function
                    return;
                }
                else {
                    shared->timer = asio::steady_timer{*_ioService, interval};
                    shared->timer.async_wait(shared->waitHandler);
                }

            }

        };

        //initiate the periodic waitHandler calls with empty error_code
        postAsync([shared] {
            shared->waitHandler(asio::error_code());
        });
    }

    uint64_t BaseIO::getUid() {
        return _uid;
    }

    void BaseIO::setOnReceive(OnReceiveValueFunc &&onRecv) {
        _onRecv = onRecv;
    }

    bool BaseIO::hasLaunched() const {
        return _hasLaunched;
    }

    void BaseIO::setIoThreadId() {
        _ioThreadId = std::this_thread::get_id();
    }

    bool BaseIO::isIoThread() const {
        return std::this_thread::get_id() == _ioThreadId;
    }

    bool BaseIO::ioThreadRunning() const {
        return (bool)_thread;
    }

}