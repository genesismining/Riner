#include <src/network/JrpcBuilder.h>

#include <gtest/gtest.h>
#include <gmock/gmock.h>

namespace miner {

TEST(JrpcBuilder, IdAsString) {
    JrpcBuilder::Options opt;
    opt.serializeIdAsString = true;
    JrpcBuilder b(opt);
    b.setId(42);
    EXPECT_EQ(42, b.getId());
    EXPECT_EQ("{\"id\":\"42\",\"jsonrpc\":\"2.0\"}", b.getJson().dump());
}

TEST(JrpcBuilder, IsAsInt) {
    JrpcBuilder::Options opt;
    opt.serializeIdAsString = false;
    JrpcBuilder b(opt);
    b.setId(42);
    EXPECT_EQ(42, b.getId());
    EXPECT_EQ("{\"id\":42,\"jsonrpc\":\"2.0\"}", b.getJson().dump());
}

} // miner
