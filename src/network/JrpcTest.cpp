
#include <src/network/JsonRpcBuilder.h>
#include <src/network/JsonRpcUtil.h>

#include <future>
#include <src/util/Barrier.h>

#include <gtest/gtest.h>
#include <gmock/gmock.h>
#include <chrono>
#include <src/util/FileUtils.h>
#include <src/util/TestSslData.h>

namespace miner {
    using namespace jrpc;
    using testing::AnyOf;
    using namespace std::chrono_literals;

    TEST(RequestBuilder, RequestProperties) {

        Message req = RequestBuilder{}
            .id(0)
            .method("methodName")
            .param("abc")
            .param(1)
            .param(2.718)
            .param(jrpc::Null{})
            .done();

        EXPECT_TRUE(req.hasMethodName("methodName"));
        EXPECT_TRUE(req.isRequest());
        EXPECT_TRUE(req.getIfRequest());

        EXPECT_FALSE(req.isError());
        EXPECT_FALSE(req.getIfError());
        EXPECT_FALSE(req.getIfResult());
        EXPECT_FALSE(req.isNotification());
        EXPECT_FALSE(req.isResponse());
        EXPECT_FALSE(req.isResultTrue());

        EXPECT_EQ(req.toJson(), R"({"id":0,"jsonrpc":"2.0","method":"methodName","params":["abc",1,2.718,null]})"_json);
    }

    TEST(JsonRpcUtil, ConnectDisconnect) {
        //this test
        //- starts a jrpc server and client
        //- connects them
        //- sends a dummy call from the client to the server
        //- disconnects the client
        //- closes the server when it realizes that the client has disconnected

        using namespace std;
        atomic_int order{0}; //counted up
        //track the order in which the following things happen:
        int serverConnected    = {0}; //curly braces in case we want to make it an atomic later
        int serverDisconnected = {0};
        int clientConnected    = {0};
        int clientDisconnected = {0};

        Barrier barrier;
        std::future_status status;

        {
            JsonRpcUtil server{IOMode::Tcp};
            JsonRpcUtil client{IOMode::Tcp};

            server.launchServer(4029, [&](CxnHandle cxn) {
                //on connect
                serverConnected = ++order; //(1 or 2)
                server.readAsync(cxn);
            }, [&] {
                //on disconnect
                serverDisconnected = ++order; //(3 or 4)

                barrier.unblock();
            });

            client.launchClient("127.0.0.1", 4029, [&](CxnHandle cxn) {
                //on connect
                clientConnected = ++order; //(1 or 2)

                //no further actions or response handler that use cxn are added, therefore the connection is dropped
                client.callAsync(cxn, RequestBuilder{}.method("DummyCall").param(0).done());
                client.readAsync(cxn);
            }, [&] {
                //on disconnect
                clientDisconnected = ++order; //(3 or 4)
            });

            status = barrier.wait_for(2s);

            EXPECT_NE(status, std::future_status::timeout);

        } //server + client dtors

        if (status != std::future_status::timeout) {
            //from here on, all the async stuff is done;
            EXPECT_EQ(order, std::max(serverDisconnected, clientDisconnected));

            //variables contain the order of events starting at 1
            EXPECT_THAT(serverConnected, AnyOf(1, 2));
            EXPECT_THAT(clientConnected, AnyOf(1, 2));
            EXPECT_NE(  clientConnected, serverConnected);

            EXPECT_THAT(serverDisconnected, AnyOf(3, 4));
            EXPECT_THAT(clientDisconnected, AnyOf(3, 4));
            EXPECT_NE(  clientDisconnected, serverDisconnected);
        }
    }

    TEST(JsonRpcUtil, Methods) {
        using RB = RequestBuilder;

        Barrier barrier;
        std::future_status status;

        std::atomic_int counter {0};

        {
            JsonRpcUtil server{IOMode::Tcp};
            JsonRpcUtil client{IOMode::Tcp};

            //auto logServer = [] (auto &) {LOG(INFO) << "                                   server:";};
            //auto logClient = [] (auto &) {LOG(INFO) << "                                   client:";};
            //server.setOutgoingModifier(logServer);
            //server.setIncomingModifier(logServer);
            //client.setOutgoingModifier(logClient);
            //client.setIncomingModifier(logClient);

            server.launchServer(4030, [&](CxnHandle cxn) {
                server.setReadAsyncLoopEnabled(true);
                server.readAsync(cxn);
            }, [&] {
                ++counter; LOG(INFO) << "server dc";
                barrier.unblock();
            });

            server.addMethod("() -> void", [&] () {
                ++counter; LOG(INFO) << "method 0";
            });

            server.addMethod("() -> 4", [&] () {
                ++counter; LOG(INFO) << "method 1";
                return 4;
            });

            server.addMethod("(4) -> 8", [&] (int n) {
                EXPECT_EQ(n, 4);
                ++counter; LOG(INFO) << "method 2";
                return 2*4;
            }, "n");

            server.addMethod("(4, 5) -> 9", [&] (int n, int m) {
                EXPECT_EQ(n, 4);
                EXPECT_EQ(m, 5);
                ++counter; LOG(INFO) << "method 3";
                return n + m;
            }, "n", "m");

            client.launchClient("127.0.0.1", 4030, [&](CxnHandle cxn) {
                client.setReadAsyncLoopEnabled(true);
                client.readAsync(cxn);

                //pure virtual function call
                client.callAsync(cxn, RB{}.id(1).method("() -> void").done(), [&] (CxnHandle cxn, Message msg) {
                    EXPECT_TRUE(msg.isResponse());
                    EXPECT_TRUE(msg.getIfResult());
                    ++counter; LOG(INFO) << "return from method 0";

                    if (auto result = msg.getIfResult()) {
                        EXPECT_TRUE(result->empty());
                        ++counter; LOG(INFO) << "got to line: " << __LINE__;
                    }

                    client.callAsync(cxn, RB{}.id(2)
                    .method("() -> 4")
                    .done(), [&] (CxnHandle cxn, Message msg) {
                        EXPECT_TRUE(msg.isResponse());
                        EXPECT_TRUE(msg.getIfResult());
                        ++counter; LOG(INFO) << "return from method 1";

                        if (auto result = msg.getIfResult()) {
                            EXPECT_EQ(*result, 4);
                            ++counter; LOG(INFO) << "got to line: " << __LINE__;
                        }

                        client.callAsync(cxn, RB{}.id(3)
                        .method("(4) -> 8")
                        .param("n", 4)
                        .done(), [&] (CxnHandle cxn, Message msg) {
                            EXPECT_TRUE(msg.isResponse());
                            EXPECT_TRUE(msg.getIfResult());
                            ++counter; LOG(INFO) << "return from method 2";

                            if (auto result = msg.getIfResult()) {
                                EXPECT_EQ(*result, 8);
                                ++counter; LOG(INFO) << "got to line: " << __LINE__;
                            }

                            client.callAsync(cxn, RB{}.id(4)
                            .method("(4, 5) -> 9")
                            .param("n", 4)
                            .param("m", 5)
                            .done(), [&] (CxnHandle cxn, Message msg) {
                                EXPECT_TRUE(msg.isResponse());
                                EXPECT_TRUE(msg.getIfResult());
                                ++counter; LOG(INFO) << "return from method 3";

                                if (auto result = msg.getIfResult()) {
                                    EXPECT_EQ(*result, 9);
                                    ++counter; LOG(INFO) << "got to line: " << __LINE__;
                                }

                                client.setReadAsyncLoopEnabled(false); //drop connection, schedule no new readAsync() call on that cxn
                            });
                        });
                    });
                });

            }, [&] {
                //on disconnect
                ++counter; LOG(INFO) << "client dc";
            });

            status = barrier.wait_for(2s);

            EXPECT_NE(status, std::future_status::timeout);
        }

        if (status != std::future_status::timeout) {
            EXPECT_EQ(counter, 14);
        }
    }

    class JsonRpcServerClientFixture: public testing::Test {
    protected:
        Barrier barrier;
        std::future_status barrier_status;

        unique_ptr<JsonRpcUtil> server;
        unique_ptr<JsonRpcUtil> client;

        SslDesc sslDescServer;
        SslDesc sslDescClient;
    public:
        using RB = jrpc::RequestBuilder;

        JsonRpcServerClientFixture()
        : server(make_unique<JsonRpcUtil>(IOMode::Tcp))
        , client(make_unique<JsonRpcUtil>(IOMode::Tcp)) {
        }

        bool initSsl() {
            bool success = true;

            std::string certPath = "/tmp/temp_server_certificate_for_testing.pem";
            std::string privPath = "/tmp/temp_server_private_key_for_testing.pem";

            success &= file::writeStringIntoFile(certPath, TestSslData::certificate_pem);
            success &= file::writeStringIntoFile(privPath, TestSslData::private_key_pem);
            if (!success)
                return false;

            LOG(INFO) << "generated " << certPath;
            LOG(INFO) << "generated " << privPath;

            sslDescServer.server = SslDesc::Server {
                    /*cert chain */ certPath,
                    /*private key*/ privPath,
                    /*tmp dh file*/ ""//"dh512.pem"
            };

            sslDescServer.server->onGetPassword = [] () -> std::string {
                return "asdf";
            };

            sslDescClient.client.emplace();
            sslDescClient.client->certFile = certPath;

            server->io().enableSsl(sslDescServer);
            client->io().enableSsl(sslDescClient);

            return true;
        }

        void launchServerWithReadLoop() {
            server->launchServer(4031, [&] (CxnHandle cxn) {
                server->setReadAsyncLoopEnabled(true);
                server->readAsync(cxn);
            });
        };

        void launchClient(std::function<void(CxnHandle)> onCxn, std::function<void()> onDc = ioOnDisconnectedNoop) {
            client->launchClient("127.0.0.1", 4031, std::move(onCxn), std::move(onDc));
        }

        void launchClientAutoReconnect(std::function<void(CxnHandle)> onCxn, std::function<void()> onDc = ioOnDisconnectedNoop) {
            client->launchClientAutoReconnect("127.0.0.1", 4031, std::move(onCxn), std::move(onDc));
        }

        template<class Func>
        bool waitAndInvoke(Barrier &barrier, Func &&onSuccess, std::chrono::milliseconds timeoutDur = 2s) {
            auto status = barrier.wait_for(timeoutDur);
            bool timedOut = status == std::future_status::timeout;
            if (!timedOut) {
                onSuccess();
            }
            return timedOut;
        }
    };

    TEST_F(JsonRpcServerClientFixture, MethodNotFound) {
        //this test calls a function that the server does not offer

        server->addMethod("myMethod", [] () {
            EXPECT_TRUE(false); //this line should never be reached
        });

        LockGuarded<Message> msg;

        launchServerWithReadLoop();
        launchClient([&] (CxnHandle cxn) {
            client->callAsync(cxn, RB{}.id(0).method("otherMethod").done(), [&] (CxnHandle cxn, Message res) {
                *msg.lock() = res;
                barrier.unblock();
            });
            client->readAsync(cxn);
        });

        bool timeout = waitAndInvoke(barrier, [&] () {
            server.reset();
            client.reset();

            {auto lmsg = msg.lock();
                EXPECT_TRUE(lmsg->isError());
                if (lmsg->isError())
                    EXPECT_EQ(lmsg->getIfError()->code, jrpc::ErrorCode::method_not_found);
            }
        });
        EXPECT_FALSE(timeout);
    }

    TEST_F(JsonRpcServerClientFixture, SslConnection) {
        //this test calls a jrpc on an ssl enabled io object
        //it does not test whether the ssl stream actually works!

#ifndef HAS_OPENSSL
        LOG(INFO) << "skip this test since the binary was compiled without openssl support";
        return;
#endif

        LockGuarded<Message> msg; //response from server to client

        EXPECT_TRUE(initSsl()); //initialize ssl for server and client members

        server->addMethod("method", [] () {
            return true;
        });

        launchServerWithReadLoop();
        launchClient([&] (CxnHandle cxn) {

            client->callAsync(cxn, RB{}.id(0).method("method").done(), [&] (CxnHandle cxn, Message res) {
                *msg.lock() = res;
                barrier.unblock();
            });
            client->readAsync(cxn);
        });

        bool timeout = waitAndInvoke(barrier, [&] () {
            server.reset();
            client.reset();

            {auto lmsg = msg.lock();
                EXPECT_TRUE(lmsg->isResultTrue());
            }
        });
        EXPECT_FALSE(timeout);
    }

    TEST_F(JsonRpcServerClientFixture, SslHandshakeWrongPassword) {
        //this test tries to call a jrpc on an ssl enabled io object with a wrong password
        //it tests whether no connection is established with a wrong password
        //it does not test whether the wrong password is the reason for the connection
        //not being established, so this test only makes sense together with other tests

        bool hasOpenSsl = false;
#ifdef HAS_OPENSSL
        hasOpenSsl = true;
#endif

        auto testDuration = hasOpenSsl ? 4s : 300ms; //shorter time if timeout is expected due to no openssl support, lets not make this test waste time on expected behavior.

        EXPECT_TRUE(initSsl()); //initialize ssl for server and client members

        sslDescServer.server->onGetPassword = [] () -> std::string {
            return "this is a wrong password";
        };

        server->io().enableSsl(sslDescServer);

        server->addMethod("method", [&] () {
            barrier.unblock();
        });

        launchServerWithReadLoop();

        launchClient([&] (CxnHandle cxn) {
            client->callAsync(cxn, RB{}.id(0).method("method").done(), [&] (CxnHandle cxn, Message res) {});
            client->readAsync(cxn);
        });

        bool timeout = waitAndInvoke(barrier, [&] () {
            server.reset();
            client.reset();
        }, testDuration);
        LOG(INFO) << "waiting over timeout = " << timeout;

        server.reset();
        client.reset();
        LOG(INFO) << "deinit";

        if (hasOpenSsl) //expect openssl == !!timeout
            EXPECT_FALSE(timeout);
        else
            EXPECT_TRUE(timeout);
    }

    TEST_F(JsonRpcServerClientFixture, JsonRpcKillConnection) {

        // timeline comment (use your IDE's comment folding to hide/unhide)
        /*
            thread is shown by the indentation used
            *--------------*-------------------*------------------
            | main thread  |  server ioThread  | client ioThread
            v              v                   v

            > launch server
            > server.addMethod "method"
            > launch client
            > wait for barrier
            --------------connection established------------------
                            > nothing ever happens on the server for this test
                                               > server->disconnectAll() called
                                               > ioThread stops
                                               > ~asio::io_service destroys captured Connection sharedPtrs
                                               > Connection dtor disconnects server from client
                                               > server->disconnectAll() returned
                                               > client->callAsync("method")
                                               > witnesses disconnect from server
                                               > unblocks barrier before its timeout
            > server.reset()
            > client.reset() called
                                               > client ioThread stops
            > client.reset() returned

            > no timeout! test succeeded
        */

        launchServerWithReadLoop();

        Barrier serverBarrier;
        auto testDuration = 5s; //successful test took 3008ms once

        server->addMethod("method", [&] () {
            EXPECT_FALSE("should not be processing this call ever, since disconnectAll was called already");
            serverBarrier.wait_for(testDuration + 10ms);
        });

        launchClient([&] (CxnHandle cxn) {
            server->io().disconnectAll(); //in this moment the server has already queued a readAsync(), which is expected to be asio::error::operation_abort'ed

            client->callAsync(cxn, RB{}.id(0).method("method").done(), [&] (CxnHandle cxn, Message res) {});
            client->readAsync(cxn);
        }, [&] {
            LOG(INFO) << "test client witnessed that server has disconnected";
            barrier.unblock();
        });

        bool timeout = waitAndInvoke(barrier, [&] () {
            server.reset();
            client.reset();
        }, testDuration);
        EXPECT_FALSE(timeout);
    }

    TEST_F(JsonRpcServerClientFixture, JsonRpcKillConnectionWhileCallRetryIsQueued) {

        // timeline comment (use your IDE's comment folding to hide/unhide)
        /*
            thread is shown by the indentation used
            *--------------*-------------------*------------------
            | main thread  |  server ioThread  | client ioThread
            v              v                   v

            > create shared_ptr of some test objects X, Y
            > launch server
            > server.addMethod "method"
            > launch client
            > wait for barrier
            --------------connection established------------------
                                               > client->call "method" retry every 10 seconds (retry is supposed to never happen in this test)
                                               > capture shared_ptr X in predicate handler and Y in onCancelled handler
                            > "method" gets invoked
                            > client->disconnectAll() called (forces client to join io thread and kill all queued retries)
                            > client ioThread stops
                            > client ~io_service() kills all handlers and their captured Connection object
                            > client disconnect
                            > client's active retires get destroyed (=> X and Y refcount decrements)
                            > client's onCancelled gets called
                            > client->disconnectAll() returns
                            > server witnesses client disconnect
                            > unblocks barrier
            > server.reset()
            > client.reset() called
                                               > client ioThread stops
            > client.reset() returned
            > check if X and Y refcount == 1 (which means the handlers got properly destroyed)
            > no timeout! test succeeded
        */

        auto X = make_shared<int>(1234); //doesn't have to be an int, just some shared ptr instance
        auto Y = make_shared<int>(5678);

        std::atomic_bool onCancelledHappened {false};
        std::atomic_bool clientWitnessedServerDc {false};

        launchServerWithReadLoop();

        auto testDuration = 4s;

        server->addMethod("method", [&] () {
            client->io().disconnectAll();
            barrier.unblock();
        });

        launchClientAutoReconnect([&] (CxnHandle cxn) {
            auto maxTries = 5;
            auto retryInterval = 10s;

            auto predicate = [&, X] (CxnHandle cxn, Message res) {
                EXPECT_FALSE("predicate: this point should never be reached");
            };

            auto onCancelled = [&, Y] {
                EXPECT_GT(X.use_count(), 1); //expect more than one shared_ptr pointing to it
                EXPECT_GT(Y.use_count(), 1);
                onCancelledHappened = true;
            };

            client->callAsyncRetryNTimes(cxn, RB{}.id(0).method("method").done(), maxTries, retryInterval, predicate, onCancelled);
            client->readAsync(cxn);
        }, [&] {
            LOG(INFO) << "test client witnessed that server has disconnected";
            clientWitnessedServerDc = true;
        });

        bool timeout = waitAndInvoke(barrier, [&] () {
            server.reset();
            client.reset();
        }, testDuration);

        EXPECT_TRUE(onCancelledHappened);
        EXPECT_TRUE(clientWitnessedServerDc);

        //check if captured X and Y shared_ptrs got cleaned up properly and only the last instances are remaining
        EXPECT_EQ(X.use_count(), 1);
        EXPECT_EQ(Y.use_count(), 1);

        EXPECT_FALSE(timeout);
    }

} // miner




















