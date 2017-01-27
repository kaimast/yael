#include "test/SocketTest.h"

using namespace yael::network;

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

    uint8_t *data_received = nullptr;
    uint32_t receive_len = 0;

    bool has_data = m_peer_socket->receive(data_received, receive_len);

    ASSERT_TRUE(has_data);

    ASSERT_EQ(len, receive_len);
    ASSERT_EQ(0, memcmp(data, data_received, len));

    ASSERT_TRUE(m_peer_socket->is_connected());
    ASSERT_TRUE(m_socket2.is_connected());

    delete []data_received;
}

TEST_F(SocketTest, send_other_way)
{
    const uint32_t len = 4313;
    uint8_t data[len];

    bool sent = m_peer_socket->send(data, len);
    ASSERT_TRUE(sent);

    uint8_t *data_received = nullptr;
    uint32_t receive_len = 0;

    bool has_data = m_socket2.receive(data_received, receive_len);

    ASSERT_TRUE(has_data);

    ASSERT_EQ(len, receive_len);
    ASSERT_EQ(0, memcmp(data, data_received, len));

    ASSERT_TRUE(m_peer_socket->is_connected());
    ASSERT_TRUE(m_socket2.is_connected());

    delete []data_received;
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

    uint8_t *data_received = nullptr;
    uint32_t receive_len = 0;

    bool has_data = m_peer_socket->receive(data_received, receive_len);

    ASSERT_TRUE(has_data);
    ASSERT_EQ(type1, *data_received);
    ASSERT_EQ(len, receive_len);

    has_data = m_peer_socket->receive(data_received, receive_len);

    ASSERT_TRUE(has_data);
    ASSERT_EQ(type2, *data_received);
    ASSERT_EQ(len, receive_len);
}
