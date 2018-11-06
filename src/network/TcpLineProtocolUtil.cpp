
#include "TcpLineProtocolUtil.h"

#include <asio.hpp>
#include <thread>
#include <src/common/Pointers.h>
#include <src/common/Assert.h>

namespace miner {

    using asio::ip::tcp;
    using asio::error_code;

    TcpLineProtocolUtil::TcpLineProtocolUtil(cstring_span hostSpan,
                                             cstring_span portSpan,
                                             OnEventFunc &&onEventFunc)
            : host(to_string(hostSpan)), port(to_string(portSpan)), onEvent(std::move(onEventFunc)),
              resolver(ioService), socket(ioService) {
    }

    void TcpLineProtocolUtil::launch() {
        tcp::resolver::query query(host, port);

        //start chain of async calls
        resolver.async_resolve(query, [this](auto &error, auto it) {
            handleResolve(error, it);
        });

        //start io service thread which will handle the async calls
        thread = std::make_unique<std::thread>([this]() {
            try {
                while(true) {
                    LOG(WARNING) << "(re) starting ioService.run()";
                    ioService.run();

                    if (shutdown) {
                        break;
                    }
                    else {
                        ioService.restart();
                    }
                    std::this_thread::sleep_for(std::chrono::seconds(1)); //todo: remove
                    //returns whenever stopped or no handlers left to execute
                }
            }
            catch (std::exception &e) {
                LOG(ERROR) << "uncaught exception in TcpLineProtocolUtil thread: " << e.what();
            }
            LOG(WARNING) << "ioService thread has finished";
        });

    }

    TcpLineProtocolUtil::~TcpLineProtocolUtil() {
        LOG(WARNING) << "TcpLineProtocolUtil destructor called";
        shutdown = true;
        ioService.stop();

        if (thread->joinable()) {
            thread->join();
        }

        if (socket.is_open()) {
            socket.close();
        }
        LOG(WARNING) << "TcpLineProtocolUtil destructor finished";
    }

    void TcpLineProtocolUtil::retry(const asio::error_code &reason) {

        if (reason == asio::error::operation_aborted) {//todo: maybe handle this in the respective retry callers
            LOG(WARNING) << "(maybe nested) retry aborted";
            return; //do not retry if the reason is an abort (likely caused by another retry invocation)
        }

        LOG(INFO) << "trying to reconnect to " << host << ":" << port;

        postAsync([this] {
            auto error = asio::error_code();

            if (socket.is_open()) {

                socket.shutdown(tcp::socket::shutdown_both, error);
                if (error) {
                    LOG(INFO) << "asio error at socket shutdown: " << error.value() << " - " << error.message();
                }

                socket.close(error);
                if (error) {
                    LOG(INFO) << "asio error at socket close: " << error.value() << " - " << error.message();
                }
            }

            socket.cancel(error);
            if (error) {
                LOG(INFO) << "asio error at socket cancel: " << error.value() << " - " << error.message();
            }

            //reset asio state
            socket = tcp::socket(ioService);
            resolver = tcp::resolver(ioService);

            std::this_thread::sleep_for(std::chrono::seconds(1)); //todo: remove this call, wait async

            tcp::resolver::query query(host, port);

            LOG(WARNING) << "async_resolve";
            resolver.async_resolve(query, [this](auto &error, auto it) {
                handleResolve(error, it);
            });
        });
    }

    void TcpLineProtocolUtil::reportAsioError(const error_code &error) {
        LOG(ERROR) << "asio error in TcpLineProtocolUtil: " << error.value() << "-" << error.message();
    }

    void TcpLineProtocolUtil::handleResolve(const error_code &error, tcp::resolver::iterator it) {
        if (!error) {
            //try connect to first endpoint, handleConnect tries the other endpoints if that fails
            tcp::endpoint endpoint = *it;
            LOG(WARNING) << "async_connect";
            socket.async_connect(endpoint, [this, it](auto &error) {
                handleConnect(error, it);
            });
        } else {
            reportAsioError(error);
            retry(error);
        }
    }

    void TcpLineProtocolUtil::handleConnect(const error_code &error, tcp::resolver::iterator it) {
        if (!error) {
            LOG(INFO) << "successfully connected.";
            //successfully connected, enter onEvent coroutine for the first time
            handleEvent(error_code(), "");
        } else if (it != tcp::resolver::iterator()) {//default constructed iterator is "end"
            //The connection failed, but there's another endpoint to try.
            socket.close();

            tcp::endpoint endpoint = *it;
            LOG(INFO) << "asio error while trying this endpoint: " << error.value() << "-" << error.message();
            LOG(INFO) << "connecting: trying different endpoint with ip: " << endpoint.address().to_string();
            socket.async_connect(endpoint, [this, next = ++it](auto &error) {
                handleConnect(error, next);
            });
        } else {
            reportAsioError(error);
            retry(error);
        }
    }

    void TcpLineProtocolUtil::handleEvent(const error_code &error, std::string line) {
        if (error == asio::error::operation_aborted) { //todo: maybe find a more elegant way than this at every handler
            LOG(WARNING) << "handleEvent aborted";
            return; //this operation was likely aborted by socket.close(), do not proceed.
        }

        LOG(WARNING) << "onEvent";
        onEvent(std::move(line), error, coroutine);

        if (error) {
            reportAsioError(error);
            retry(error);
        }
    }

    void TcpLineProtocolUtil::asyncWrite(std::string requestLine, bool reenterCoroutine) {

        std::ostream stream(&request);
        stream << requestLine;

        LOG(WARNING) << "async_write";
        asio::async_write(socket, request, [this, reenterCoroutine] (auto &error, size_t numBytes) {

            if (reenterCoroutine || error) {
                handleEvent(error, "");
            }
        });
    }

    /*
    void TcpLineProtocolUtil::asyncRead() {
        //read until '\n'
        LOG(WARNING) << "async_read_until";
        asio::async_read_until(socket, response, "\n", [this] (auto &error, size_t numBytes) {

            std::string line = "asio error occured"; //write something the api user will notice in case they forget to check the asio::error

            if (!error && numBytes != 0) {
                std::istream stream(&response);
                std::getline(stream, line);
            }

            MI_EXPECTS(line.size() == strlen(line.c_str()));

            handleEvent(error, line);
        });
    }
*/
    void TcpLineProtocolUtil::asyncRead(int debugInt) {
        //read until '\n'
        auto currentNumAsyncReads = numAsyncReads += debugInt;
        LOG(WARNING) << "async_read_until " << std::this_thread::get_id() << " val: " << currentNumAsyncReads;
        MI_EXPECTS(currentNumAsyncReads <= 20000);
        asio::async_read_until(socket, response, '\n', [this, debugInt] (auto &error, size_t numBytes) {
            numAsyncReads -= debugInt;

            std::string line = "asio line error occured"; //write something the api user will notice in case they forget to check the asio::error

            if (!error) {
                if (numBytes != 0) {

                    std::istream stream(&response);

                    std::vector<char> buf(numBytes);
                    stream.read(buf.data(), buf.size());
                    line = std::string(buf.data(), buf.size());

                    if (line[line.size() - 1] != '\n') {
                        LOG(INFO) << "async_read_until: line does not end with \\n, line: " << line;
                    }
                    //std::getline(stream, line);
                }
                else {
                    LOG(INFO) << "NUMBYTES IS ZERO";
                }
            }

            MI_EXPECTS(line.size() == strlen(line.c_str()));

            handleEvent(error, line);
        });

    }

    void TcpLineProtocolUtil::asyncRetryEvery(std::chrono::milliseconds interval, std::function<bool()> &&pred) {

        auto shared = std::shared_ptr<AsyncRetry>(new AsyncRetry{
                {ioService, interval}, std::move(pred), {}, {}
        });

        auto weak = make_weak(shared);

        {
            auto list = activeRetries.lock();
            list->push_front(shared);
            shared->it = list->begin();
        }

        shared->waitHandler = [this, weak, interval]
                (const error_code &error) {
            LOG(INFO) << "Wait handler start";

            if (error) {
                LOG(INFO) << "error " << error;
                return;
            }

            if (auto shared = weak.lock()) {

                if (shared->pred()) {
                    LOG(INFO) << "erase ";

                    activeRetries.lock()->erase(shared->it); //'shared' is now the last ref

                    //'shared' is the last reference to this function, do not let this
                    //function continue in a destroyed state
                    return;
                }
                else {
                    shared->timer = asio::steady_timer{ioService, interval};
                    LOG(WARNING) << "async_wait";
                    shared->timer.async_wait(shared->waitHandler);
                }

            }

        };

        //initiate the periodic waitHandler calls with empty error_code
        postAsync([shared] {
            shared->waitHandler(asio::error_code());
        });
    }

}