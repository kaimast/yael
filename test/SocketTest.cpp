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
    char data[len];

    bool sent = m_socket2.send(data, len);
    ASSERT_TRUE(sent);

    char *data_received = nullptr;
    uint32_t receive_len = 0;

    bool has_data = m_peer_socket->receive(data_received, receive_len);

    ASSERT_TRUE(has_data);

    ASSERT_EQ(len, receive_len);
    ASSERT_EQ(0, memcmp(data, data_received, len));

    ASSERT_TRUE(m_peer_socket->is_connected());
    ASSERT_TRUE(m_socket2.is_connected());

    delete []data_received;
}

TEST_F(SocketTest, multiple_datagrams)
{
    const uint32_t len1 = 4313;
    char data1[len1];

    const uint32_t len2 = 413;
    char data2[len2];

    Socket::message_out_t message;
    message.emplace_back(Socket::datagram_out_t{data1, len1});
    message.emplace_back(Socket::datagram_out_t{data2, len2});

    bool sent = m_socket2.send(message);
    ASSERT_TRUE(sent);

    Socket::message_in_t message_in;
    bool has_data = m_peer_socket->receive(message_in);

    ASSERT_TRUE(has_data);

    ASSERT_EQ(2, message.size());
    ASSERT_EQ(len1, message_in[0].length);
    ASSERT_EQ(len2, message_in[1].length);

    ASSERT_TRUE(m_peer_socket->is_connected());
    ASSERT_TRUE(m_socket2.is_connected());

    Socket::free_message(message_in);
}

TEST_F(SocketTest, send_other_way)
{
    const uint32_t len = 4313;
    char data[len];

    bool sent = m_peer_socket->send(data, len);
    ASSERT_TRUE(sent);

    char *data_received = nullptr;
    uint32_t receive_len = 0;

    bool has_data = m_socket2.receive(data_received, receive_len);

    ASSERT_TRUE(has_data);

    ASSERT_EQ(len, receive_len);
    ASSERT_EQ(0, memcmp(data, data_received, len));

    ASSERT_TRUE(m_peer_socket->is_connected());
    ASSERT_TRUE(m_socket2.is_connected());

    delete []data_received;
}

TEST_F(SocketTest, peek)
{
    const uint32_t len = 24324;
    char data[len];

    bool sent = m_socket2.send(data, len);
    ASSERT_TRUE(sent);

    Socket::datagram_in_t peeked;
    bool res = m_peer_socket->peek(peeked);

    ASSERT_EQ(true, res);
    ASSERT_EQ(len, peeked.length);
    ASSERT_EQ(0, memcmp(data, peeked.data, len));
    ASSERT_TRUE(m_peer_socket->has_messages());
}

TEST_F(SocketTest, fileno)
{
    ASSERT_TRUE(m_socket2.get_fileno() >= 0);
}

TEST_F(SocketTest, first_in_first_out)
{
    const char type1 = 12;
    const char type2 = 42;
    const uint32_t len = 1;

    bool sent = m_socket2.send(&type1, len);
    ASSERT_TRUE(sent);
    sent = m_socket2.send(&type2, len);
    ASSERT_TRUE(sent);

    char *data_received = nullptr;
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
