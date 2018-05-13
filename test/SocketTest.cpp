#include <thread>
#include <gtest/gtest.h>
#include <yael/network/TcpSocket.h>

using namespace yael::network;

class SocketTest : public testing::Test
{
protected:
    void SetUp() override
    {
        Address addr = resolve_URL("localhost", 62123);
        bool listening = m_socket1.listen(addr.IP, addr.PortNumber, 10);
        ASSERT_TRUE(listening);

        bool connected = m_socket2.connect(addr);
        ASSERT_TRUE(connected);

        m_peer_socket = nullptr;
        
        while(m_peer_socket == nullptr)
        {
            auto socks = m_socket1.accept();

            if(socks.size() > 0)
            {
                m_peer_socket = socks[0];
            }
        }
    }

    void TearDown() override
    {
        delete m_peer_socket;
    }

    TcpSocket m_socket1, m_socket2;
    Socket *m_peer_socket;

};

TEST_F(SocketTest, listening)
{
    EXPECT_TRUE(m_socket1.is_listening());
    EXPECT_TRUE(m_socket1.is_valid());
    EXPECT_FALSE(m_socket1.is_connected());
    EXPECT_EQ(62123, m_socket1.port());
}

TEST_F(SocketTest, connected)
{
    EXPECT_TRUE(m_socket2.is_valid());
    EXPECT_TRUE(m_socket2.is_connected());
    EXPECT_TRUE(m_peer_socket != nullptr);
    EXPECT_TRUE(m_socket1.port() != m_socket2.port());
}

TEST_F(SocketTest, send_one_way)
{
    const uint32_t len = 4313;
    uint8_t data[len];

    bool sent = m_socket2.send(data, len);
    ASSERT_TRUE(sent);

    std::optional<Socket::message_in_t> msg;

    while(!msg)
    {
        msg = m_peer_socket->receive();
    }

    ASSERT_EQ(len, msg->length);
    ASSERT_EQ(0, memcmp(data, msg->data, len));

    ASSERT_TRUE(m_peer_socket->is_connected());
    ASSERT_TRUE(m_socket2.is_connected());
}

TEST_F(SocketTest, send_other_way)
{
    const uint32_t len = 4313;
    uint8_t data[len];

    bool sent = m_peer_socket->send(data, len);
    ASSERT_TRUE(sent);

    std::optional<Socket::message_in_t> msg;

    while(!msg)
    {
        msg = m_socket2.receive();
    }
 
    ASSERT_EQ(msg->length, len);
    ASSERT_EQ(0, memcmp(data, msg->data, msg->length));

    ASSERT_TRUE(m_peer_socket->is_connected());
    ASSERT_TRUE(m_socket2.is_connected());
}

TEST_F(SocketTest, fileno)
{
    ASSERT_TRUE(m_socket2.get_fileno() >= 0);
}

TEST_F(SocketTest, first_in_first_out)
{
    const uint8_t type1 = 12;
    const uint8_t type2 = 42;
    const uint32_t len = 1;

    bool sent = m_socket2.send(&type1, len);
    ASSERT_TRUE(sent);
    sent = m_socket2.send(&type2, len);
    ASSERT_TRUE(sent);

    std::optional<Socket::message_in_t> msg1;

    while(!msg1)
    {
        msg1 = m_peer_socket->receive();
    }
 
    std::optional<Socket::message_in_t> msg2;

    while(!msg2)
    {
        msg2 = m_peer_socket->receive();
    }

    ASSERT_EQ(type1, *msg1->data);
    ASSERT_EQ(len, msg1->length);
    ASSERT_EQ(type2, *msg2->data);
    ASSERT_EQ(len, msg2->length);
}
