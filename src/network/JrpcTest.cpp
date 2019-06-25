
#include <src/network/JsonRpcBuilder.h>
#include <src/network/JsonRpcUtil.h>

#include <future>
#include <src/util/Barrier.h>

#include <gtest/gtest.h>
#include <gtest/gtest-matchers.h>
#include <gmock/gmock.h>

namespace miner {
    using namespace jrpc;
    using testing::AnyOf;

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

        EXPECT_EQ(req.str(), R"({"id":0,"jsonrpc":"2.0","method":"methodName","params":["abc",1,2.718,null]})");
    }

    TEST(JsonRpcUtil, Connect_Disconnect) {
        //this test
        //- starts a jrpc server and client
        //- connects them
        //- sends a dummy call from the client to the server
        //- disconnects the client
        //- closes the server when it realizes that the client has disconnected

        using namespace std;
        atomic_int order{0}; //counted up
        //track the order in which the following things happen:
        int serverConnected = 0;
        int serverDisconnected = 0;
        int clientConnected = 0;
        int clientDisconnected = 0;

        Barrier barrier;

        {
            JsonRpcUtil server{IOMode::Tcp};
            JsonRpcUtil client{IOMode::Tcp};

            server.launchServer(4028, [&](CxnHandle cxn) {
                //on connect
                serverConnected = ++order; //(0 or 1)
                server.readAsync(cxn);
            }, [&] {
                //on disconnect
                serverDisconnected = ++order; //(2 or 3)

                barrier.unblock();
            });

            client.launchClient("127.0.0.1", 4028, [&](CxnHandle cxn) {
                //on connect
                clientConnected = ++order; //(0 or 1)

                //no further actions or response handler that use cxn are added, therefore the connection is dropped
                client.callAsync(cxn, RequestBuilder{}.method("DummyCall").param(0).done());
            }, [&] {
                //on disconnect
                clientDisconnected = ++order; //(2 or 3)
            });

            barrier.wait();
        }
        //from here on, all the async stuff is done;
        EXPECT_EQ(order, std::max(serverDisconnected, clientDisconnected));

        //variables contain the order of events starting at 1
        EXPECT_THAT(serverConnected, AnyOf(1, 2));
        EXPECT_THAT(clientConnected, AnyOf(1, 2));
        EXPECT_NE(clientConnected, serverConnected);

        EXPECT_THAT(serverDisconnected, AnyOf(3, 4));
        EXPECT_THAT(clientDisconnected, AnyOf(3, 4));
        EXPECT_NE(clientDisconnected, serverDisconnected);
    }

} // miner
