#include <gtest/gtest.h>
#include "nebula/network/ArchiveServer.hpp"

namespace nebula {
namespace test {

TEST(ArchiveServerTest, DefaultConfig) {
    network::ArchiveServer server;
    EXPECT_FALSE(server.isRunning());
    EXPECT_EQ(server.port(), 9871);
}

TEST(ArchiveServerTest, CustomConfig) {
    network::ArchiveServer::Config config;
    config.port = 9999;
    config.bindAddress = "0.0.0.0";
    network::ArchiveServer server(config);
    EXPECT_EQ(server.port(), 9999);
}

TEST(ArchiveServerTest, StartStop) {
    network::ArchiveServer::Config config;
    config.port = 19876;
    network::ArchiveServer server(config);

    auto ec = server.start();
    if (!ec) {
        EXPECT_TRUE(server.isRunning());
        server.stop();
        EXPECT_FALSE(server.isRunning());
    }
}

TEST(ArchiveServerTest, DoubleStart) {
    network::ArchiveServer::Config config;
    config.port = 19877;
    network::ArchiveServer server(config);

    auto ec = server.start();
    if (!ec) {
        ec = server.start();
        EXPECT_TRUE(ec);
        server.stop();
    }
}

} // namespace test
} // namespace nebula
