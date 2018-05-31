#include <thread>
#include <gtest/gtest.h>
#include <optional>
#include <list>
#include <yael/EventLoop.h>
#include <yael/NetworkSocketListener.h>
#include <yael/network/TcpSocket.h>
#include <yael/network/TlsSocket.h>

using namespace yael;
using namespace yael::network;

class Connection: public yael::NetworkSocketListener
{
public:
    Connection(const Address &addr, ProtocolType type)
    {
        Socket *socket;
 
        if(type == ProtocolType::TCP)
        {
            socket = new TcpSocket();
        }
        else
        {
            socket = new TlsSocket();
        }

        if(!socket->connect(addr))
        {
            throw std::runtime_error("Connection failed");
        }

        NetworkSocketListener::set_socket(std::unique_ptr<Socket>(socket), SocketType::Connection);
    }

    explicit Connection() {}

    Connection(const Connection &other) = delete;

    using NetworkSocketListener::set_socket;

    std::optional<Socket::message_in_t> receive()
    {   
        lock();
        std::optional<Socket::message_in_t> out = {};

        if(!m_messages.empty())
        {
            out = m_messages.front();
            m_messages.pop_front();
        }

        unlock();
        return out;
    }

    void on_network_message(Socket::message_in_t &msg)
    {
        m_messages.push_back(msg);
    }

private:
    std::list<Socket::message_in_t> m_messages;
};

class Server : public yael::NetworkSocketListener
{
public:
    Server(const Address &addr, std::shared_ptr<Connection> &conn, ProtocolType type)
        : m_connection(conn)
    {
        Socket *socket;
 
        if(type == ProtocolType::TCP)
        {
            socket = new TcpSocket();
        }
        else
        {
            socket = new TlsSocket("../test/test.key", "../test/test.cert");
        }

        if(!socket->listen(addr, 10))
        {
            throw std::runtime_error("Cannot start server: listen failed");
        }

        NetworkSocketListener::set_socket(std::unique_ptr<Socket>(socket), SocketType::Acceptor);
    }

    void on_new_connection(std::unique_ptr<Socket> &&socket) override
    {
        m_connection->set_socket(std::move(socket), SocketType::Connection);

        auto &el = EventLoop::get_instance();
        el.register_event_listener(m_connection);
    }

private:
    std::shared_ptr<Connection> m_connection;
};

class SocketTest : public testing::TestWithParam<ProtocolType>
{
protected:
    void SetUp() override
    {
        EventLoop::initialize();
        auto &el = EventLoop::get_instance();

        const Address addr = resolve_URL("localhost", 62123);

        m_connection1 = el.allocate_event_listener<Connection>();
        m_server = el.make_event_listener<Server>(addr, m_connection1, GetParam());
    
        m_connection2 = el.make_event_listener<Connection>(addr, GetParam());

        m_connection1->wait_for_connection();
        m_connection2->wait_for_connection();
    }

    void TearDown() override
    {
        // drop references
        m_server = nullptr;
        m_connection1 = nullptr;
        m_connection2 = nullptr;

        // shut down worker threads
        auto &el = EventLoop::get_instance();
        el.stop();
        el.wait();

        EventLoop::destroy();
    }

    std::shared_ptr<Server>     m_server = nullptr;
    std::shared_ptr<Connection> m_connection1 = nullptr;
    std::shared_ptr<Connection> m_connection2 = nullptr;
};

TEST_P(SocketTest, send_one_way)
{
    const uint32_t len = 4313;
    uint8_t data[len];

    bool sent = m_connection2->send(data, len);
    ASSERT_TRUE(sent);

    std::optional<Socket::message_in_t> msg;

    while(!msg)
    {
        msg = m_connection1->receive();
    }

    ASSERT_EQ(len, msg->length);
    ASSERT_EQ(0, memcmp(data, msg->data, len));

    delete[] msg->data;
}

TEST_P(SocketTest, send_large_chunk)
{
    const uint32_t len = 50 * 1000 * 1000;
    auto data = new uint8_t[len];

    bool sent = m_connection2->send(data, len);
    ASSERT_TRUE(sent);

    std::optional<Socket::message_in_t> msg;

    while(!msg)
    {
        msg = m_connection1->receive();
    }

    ASSERT_EQ(len, msg->length);
    ASSERT_EQ(0, memcmp(data, msg->data, len));

    delete[] msg->data;
    delete[] data;
}

TEST_P(SocketTest, send_other_way)
{
    const uint32_t len = 4313;
    uint8_t data[len];

    bool sent = m_connection1->send(data, len);
    ASSERT_TRUE(sent);

    std::optional<Socket::message_in_t> msg;

    while(!msg)
    {
        msg = m_connection2->receive();
    }

    ASSERT_EQ(len, msg->length);
    ASSERT_EQ(0, memcmp(data, msg->data, len));

    delete[] msg->data;
}

TEST_P(SocketTest, first_in_first_out)
{
    const uint8_t type1 = 12;
    const uint8_t type2 = 42;
    const uint32_t len = 1;

    bool sent = m_connection2->send(&type1, len);
    ASSERT_TRUE(sent);
    sent = m_connection2->send(&type2, len);
    ASSERT_TRUE(sent);

    std::optional<Socket::message_in_t> msg1;

    while(!msg1)
    {
        msg1 = m_connection1->receive();
    }
 
    std::optional<Socket::message_in_t> msg2;

    while(!msg2)
    {
        msg2 = m_connection1->receive();
    }

    ASSERT_EQ(type1, *msg1->data);
    ASSERT_EQ(len, msg1->length);
    ASSERT_EQ(type2, *msg2->data);
    ASSERT_EQ(len, msg2->length);

    delete[] msg1->data;
    delete[] msg2->data;
}

INSTANTIATE_TEST_CASE_P(SocketTests, SocketTest,
        testing::Values(ProtocolType::TCP, ProtocolType::TLS));
