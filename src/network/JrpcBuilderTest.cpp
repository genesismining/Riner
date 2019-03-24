#include <src/network/JrpcBuilder.h>

#include <gtest/gtest.h>
#include <gmock/gmock.h>

namespace miner {


TEST(JrpcBuilder, Id) {
    JrpcBuilder::Options opt;
    JrpcBuilder b(opt);
    b.setId(42);
    EXPECT_EQ(42, b.getId());
    EXPECT_EQ("{\"id\":42,\"jsonrpc\":\"2.0\"}", b.getJson().dump());
}

} // miner
