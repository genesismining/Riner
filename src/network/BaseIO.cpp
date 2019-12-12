//
//

#include <src/common/Assert.h>
#include <src/util/AsioErrorUtil.h>
#include "BaseIO.h"

#ifdef HAS_OPENSSL
#include <asio/ssl.hpp>
#endif

namespace miner {

    using asio::ip::tcp;

    //TODO: remove IOConnection base class. The Socket abstraction now does what was originally thought to be the responsibility of this
    //class hierarchy.
    struct Connection : public IOConnection, public std::enable_shared_from_this<Connection> {
        IOOnDisconnectedFunc &_onDisconnected;
        BaseIO::OnReceiveValueFunc &_onRecv;
        unique_ptr<Socket> _socket;
        asio::streambuf _incoming;
        uint64_t _baseIOUid = 0;

        Connection(decltype(_onDisconnected) &onDisconnected, decltype(_onRecv) &onRecv, unique_ptr<Socket> socket, uint64_t baseIOUid)
        : _onDisconnected(onDisconnected), _onRecv(onRecv), _socket(std::move(socket)), _baseIOUid(baseIOUid) {
        };

        ~Connection() override {
            LOG(INFO) << this << " closing connection (likely because no read/write operations are queued on it)";
            MI_EXPECTS(_onDisconnected);
            _onDisconnected();
        }

        uint64_t getAssociatedBaseIOUid() override {
            MI_EXPECTS(_baseIOUid != 0); //baseIO Uids start at 1, if it's 0, it wasn't set
            return _baseIOUid;
        }

        void asyncWrite(std::string outgoing) override {
            //capturing shared = shared_from_this() is critical here, it means that as long as the handler is not invoked
            //the refcount of this connection is kept above zero, and thus the connection is kept alive.
            //as soon as no async read or write action is queued on a given connection is, the connection will close itself
            asio::async_write(_socket->get(), asio::buffer(outgoing), [outgoing, shared = this->shared_from_this()] (const asio::error_code &error, size_t numBytes) {
                if (error) {
                    LOG(INFO) << "async write error " << asio_error_name_num(error) <<
                              " in Connection while trying to send '" << outgoing << "'";
                }
            });
        }

        void asyncRead() override {
            //capturing shared = shared_from_this() is critical here, it means that as long as the handler is not invoked
            //the refcount of this connection is kept above zero, and thus the connection is kept alive.
            //as soon as no async read or write action is queued on a given connection is, the connection will close itself
            LOG(INFO) << "ReadUntil queued";
            asio::async_read_until(_socket->get(), _incoming, '\n', [this, shared = this->shared_from_this()]
                    (const asio::error_code &error, size_t numBytes) {

                LOG(INFO) << "ReadUntil scheduled";

                if (error) {
                    LOG(INFO) << "asio error " << asio_error_name_num(error) << " during async_read_until: " << error.message();
                    return;
                }

                std::string line = "asio read_until error occured: #" + std::to_string(error.value()) + " " + error.message() + "\n";

                if (!error && numBytes != 0) {
                    std::istream stream(&_incoming);
                    std::getline(stream, line);
                }

                MI_EXPECTS(line.size() == strlen(line.c_str()));

                MI_EXPECTS(_onRecv);
                _onRecv(CxnHandle{shared} //becomes weak_ptr<IOConnection> aka CxnHandle
                        , line);
            });
        }

    };

    BaseIO::~BaseIO() {
        MI_EXPECTS(!isIoThread());
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
            LOG(INFO) << "called writeAsync on connection that was closed";
        }
    }

    void BaseIO::readAsync(CxnHandle handle) {
        if (auto cxn = handle.lock(*this)) {
            cxn->asyncRead(); //cxn shared_ptr is captured inside this func's async handler (which prolongs it's lifetime)
        }
        else {
            LOG(INFO) << "called readAsync on connection that was closed";
        }
    }

    uint64_t BaseIO::generateUid() {
        static std::atomic<uint64_t> nextUid = {1};
        return nextUid++;
    }

    void BaseIO::enableSsl(const SslDesc &desc) {
        _sslDesc = desc;
#ifndef HAS_OPENSSL
        LOG(ERROR) << "cannot enableSsl() on BaseIO since binary was compiled without OpenSSL support. Recompilation with OpenSSL required.";
#endif
    }

    bool BaseIO::sslEnabledButNotSupported() {
#ifdef HAS_OPENSSL
        return false;
#endif
        return _sslDesc.has_value();
    }

    void BaseIO::startIOThread() {
        MI_EXPECTS(_thread == nullptr);
        if (_thread) {
            LOG(ERROR) << "BaseIO::launchIoThread called after ioService thread was already started by another call. Ignoring this call.";
            return;
        }

        //start io service thread which will handle the async calls
        _thread = std::make_unique<std::thread>([this] () {
            setIoThreadId(); //set this thread id to be the IO Thread
            _shutdown = false; //reset shutdown to false if it remained true e.g. due to disconnectAll();

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
        MI_ENSURES(_thread);
        LOG(INFO) << "BaseIO::startIOThread";
    }

    void BaseIO::stopIOThread() { //TODO: atm subclasses are required to call stopIOThread from their dtor but implementors can forget to do so. (which causes crashes that are hard to pin down)
        // A proper way of implementing this would be to have a std::lock_guard style RAII object that guards the io thread's scope on user side.
        // e.g. launchClient returns an IOThreadGuard which has a dtor that allows the io thread to stop

        if (_thread) {
            _shutdown = true;

            _ioService->stop();

            if (_thread) {
                MI_EXPECTS(_thread->joinable());
                _thread->join();
                _thread = nullptr; //so that assert(!_thread) works as expected
            }
            _hasLaunched = false;
            //_shutdown = false; //shutdown should NOT be set to false here, since the onDisconnect handler checks _shutdown to decide whether to autoReconnect. Instead it gets set to false upon iothread restart
        }
        MI_ENSURES(!_thread);
    }

    //used by client and server
    void BaseIO::createCxnWithSocket(unique_ptr<Socket> sock) { //TODO: make method const?
        MI_EXPECTS(sock);
        auto cxn = make_shared<Connection>(_onDisconnected, _onRecv, std::move(sock), _uid);
        MI_EXPECTS(_onConnected);
        _onConnected(CxnHandle{cxn}); //user is expected to use cxn here with other calls like readAsync(cxn)
    } //cxn refcount decremented and maybe destroyed if cxn was not used in _onConnected(cxn)

    //functions used by server
    void BaseIO::launchServer(uint16_t port, IOOnConnectedFunc &&onCxn, IOOnDisconnectedFunc &&onDc) {
        if (sslEnabledButNotSupported())
            return;

        MI_EXPECTS(_ioService);
        MI_EXPECTS(_thread);
        MI_EXPECTS(!hasLaunched());
        _hasLaunched = true;
        LOG(INFO) << "BaseIO::launchServer: hasLaunched: " << hasLaunched() << " _thread: " << !!_thread;

        _onConnected    = std::move(onCxn);
        _onDisconnected = std::move(onDc);
        _acceptor = make_unique<tcp::acceptor>(*_ioService, tcp::endpoint{tcp::v4(), port});

        bool isClient = false;
        prepareSocket(isClient);
        serverListen();
    }

    void BaseIO::serverListen() {
        MI_EXPECTS(_acceptor);
        MI_EXPECTS(_socket);
        _acceptor->async_accept(_socket->get(), [this] (const asio::error_code &error) { //socket operation
            if (!error) {
                bool isClient = false;

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
        MI_EXPECTS(_ioService);
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

        MI_EXPECTS(_ioService);
        MI_EXPECTS(_thread);
        MI_EXPECTS(!hasLaunched());
        _hasLaunched = true;
        LOG(INFO) << "BaseIO::launchClient: hasLaunched: " << hasLaunched() << " _thread: " << !!_thread;

        _onConnected    = std::move(onCxn);
        _onDisconnected = [&, onDc = std::move(onDc)] () {
            _hasLaunched = false;
            onDc();
        };
        MI_ENSURES(_onDisconnected);
        MI_ENSURES(_onConnected);

        _resolver = make_unique<tcp::resolver>(*_ioService);
        bool isClient = true;
        prepareSocket(isClient);

        tcp::resolver::query query{host, std::to_string(port)};

        _resolver->async_resolve(query, [this] (auto &error, auto it) {
            if (!error) {
                auto endpoint = *it;
                _socket->get().async_connect(endpoint, [this, it] (auto &error) { //socket operation
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

            //std::function that the upcoming lambda will be converted to demands copyability. so no unique_ptrs may be captured.
            auto suSock = make_shared<unique_ptr<Socket>>(move(_socket));

            (*suSock)->asyncHandshakeOrNoop([this, suSock] (const asio::error_code &error) mutable {
                if (*suSock) {
                    handshakeHandler(error, std::move(*suSock));
                }
            }, it->host_name());
        }
        else if (it != tcp::resolver::iterator()) {//default constructed iterator is "end"
            //The connection failed, but there's another endpoint to try.
            _socket->get().close(); //socket operation

            tcp::endpoint endpoint = *it;
            LOG(INFO) << "asio error while trying this endpoint: " << error.value() << " , " << error.message();
            LOG(INFO) << "connecting: trying different endpoint with ip: " << endpoint.address().to_string();
            _socket->get().async_connect(endpoint, [this, next = ++it] (const auto &error) { //socket operation
                clientIterateEndpoints(error, next);
            });
        }
        else {
            LOG(INFO) << "asio async connect: Error " << asio_error_name_num(error) << ": " << error.message();
            _onDisconnected();
        }
    }

    //used by client and server
    //takes ownership of a socket, so that in the case of being called by the server, the server can already start preparing a new
    //socket and keep listening while the async handshake is not yet finished (this is not the call responsible for that, but the lambda encapsulating it)
    void BaseIO::handshakeHandler(const asio::error_code &error, unique_ptr<Socket> sock) { //TODO: make method const?
        if (!error) {
            if (_sslDesc) //if ssl is enabled a handshake happened successfully, otherwise nothing happened... successfully!
                LOG(INFO) << "successfully performed handshake";
            createCxnWithSocket(std::move(sock));
        }
        else {
            if (error)
                LOG(INFO) << "asio async " << (sock->isClient() ? "client" : "server") << " handshake: Error #" << error << ": " << error.message();
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

        MI_ENSURES(_onDisconnected);
        MI_ENSURES(_onConnected);
        connect(); //make initial connection
    }

    void BaseIO::disconnectAll() {
        LOG(INFO) << "BaseIO: disconnectAll";
        MI_EXPECTS(!isIoThread());

        stopIOThread();
        resetIoService();
        startIOThread();
    }

    void BaseIO::abortAllAsyncRetries() {

        //expect no handlers to be running right now!
        MI_EXPECTS(!ioThreadRunning());
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