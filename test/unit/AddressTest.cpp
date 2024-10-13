#include <gtest/gtest.h>
#include <yael/network/Address.h>

using namespace yael::network;

class AddressTest : public testing::Test
{
};

TEST(AddressTest, resovleUrl_fromIP)
{
    const std::string ip = "8.8.8.8";

    const Address addr = resolve_URL(ip, 1234, false);

    EXPECT_EQ(ip, addr.IP);
}

TEST(AddressTest, resolveUrl_localhost)
{
    const std::string url = "localhost";
    const std::string ip = "127.0.0.1";

    const Address addr= resolve_URL(url, 1234, false);

    EXPECT_EQ(ip, addr.IP);
}

TEST(AddressTest, resolveUrl_cornell)
{
    const std::string url = "www.cs.cornell.edu";
    const std::string ip = "132.236.207.53";

    const Address addr = resolve_URL(url, 1234, false);

    EXPECT_EQ(ip, addr.IP);
}
