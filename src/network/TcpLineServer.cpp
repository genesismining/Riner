

#include "TcpLineServer.h"
#include <src/util/Logging.h>
#include <src/common/Assert.h>

namespace miner {

    TcpLineServer::TcpLineServer(uint64_t port, OnEventFunc func)
    : acceptor(ioService, tcp::endpoint{tcp::v4(), static_cast<unsigned short>(port)}) {
        onEvent = std::make_shared<OnEventFunc>(std::move(func));
    }

    TcpLineServer::~TcpLineServer() {
        shutdown = true;
        ioService.stop();

        if (thread->joinable()) {
            thread->join();
        }
    }

    void TcpLineServer::launch() {
        MI_EXPECTS(thread == nullptr); //don't call launch twice on the same object!

        //start io service thread which will handle the async calls
        thread = std::make_unique<std::thread>([this] () {
            try {
                while(true) {
                    ioService.run();

                    if (shutdown) {
                        break;
                    }
                    else {
                        ioService.restart();
                    }
                    std::this_thread::sleep_for(std::chrono::seconds(1));
                    //returns whenever stopped or no handlers left to execute
                }
            }
            catch (std::exception &e) {
                LOG(ERROR) << "uncaught exception in TcpLineProtocolUtil thread: " << e.what();
            }
        });

        listen();
    }

    void TcpLineServer::listen() {

        auto socketPtr = std::make_unique<tcp::socket>(ioService);
        tcp::socket &socket = *socketPtr; //keep a ref before moving it to conn

        auto conn = std::make_shared<TcpLineConnection>(std::move(socketPtr));

        acceptor.async_accept(socket, [this, conn] (const asio::error_code &error) {
            if (!error) {
                conn->start(conn);
            }
            listen(); //TODO: check if this is an infinite recursion, verify whether asio calls this handler with an error if the ioservice is about to be destroyed
        });
    }

    void TcpLineConnection::start(std::shared_ptr<TcpLineConnection> sharedThis) {
        listen(std::move(sharedThis));
    }

    void TcpLineConnection::listen(std::shared_ptr<TcpLineConnection> sharedThis) {

        asio::async_read_until(*socket, request, '\n', [this, sharedThis = std::move(sharedThis)]
        (const asio::error_code &error, size_t numBytes) {

            std::string line = "asio error occured"; //write something the api user will notice in case they forget to check the asio::error

            if (!error && numBytes != 0) {
                std::istream stream(&request);
                std::getline(stream, line);
            }

            MI_EXPECTS(line.size() == strlen(line.c_str()));

            (*onEvent)(line, *sharedThis);

            sharedThis->start(sharedThis);
        });

    }

    void TcpLineConnection::asyncWrite(std::string response) {
        asio::async_write(*socket, asio::buffer(response), [response] (const asio::error_code &error, size_t numBytes) {
            LOG(INFO) << "async write error #" << error <<
            " in TcpLineConnection while trying to send '" << response << "'";
        });
    }

    TcpLineConnection::TcpLineConnection(unique_ptr<tcp::socket> socket)
    : socket(std::move(socket)) {
    }

}