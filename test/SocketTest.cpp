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
    static constexpr size_t MAX_SEND_QUEUE_SIZE = 10 * 1024 * 1024;

    Connection(const Address &addr, ProtocolType type)
    {
        Socket *socket;
 
        if(type == ProtocolType::TCP)
        {
            socket = new TcpSocket(MessageMode::Datagram, MAX_SEND_QUEUE_SIZE);
        }
        else
        {
            socket = new TlsSocket(MessageMode::Datagram, "", "", MAX_SEND_QUEUE_SIZE);
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
            socket = new TcpSocket(MessageMode::Datagram, Connection::MAX_SEND_QUEUE_SIZE);
        }
        else
        {
            socket = new TlsSocket(MessageMode::Datagram, "../test/test.key", "../test/test.cert", Connection::MAX_SEND_QUEUE_SIZE);
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
    static constexpr uint16_t PORT = 62123;

    void SetUp() override
    {
        EventLoop::initialize();
        auto &el = EventLoop::get_instance();

        const Address addr = resolve_URL("localhost", PORT);

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

TEST_P(SocketTest, remote_address)
{
    EXPECT_EQ(PORT, m_connection1->socket().port());
    EXPECT_EQ(PORT, m_connection2->socket().get_remote_address().PortNumber);
}

TEST_P(SocketTest, send_one_way)
{
    const uint32_t len = 4313;
    auto data = new uint8_t[len];

    m_connection2->send(data, len);

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
    uint8_t *data = new uint8_t[len];

    auto to_send = new uint8_t[len];
    memcpy(to_send, data, len);

    m_connection2->send(to_send, len);

    std::optional<Socket::message_in_t> msg;

    while(!msg)
    {
        msg = m_connection1->receive();
    }

    ASSERT_EQ(len, msg->length);
    ASSERT_EQ(0, memcmp(data, msg->data, len));

    ASSERT_EQ(0, m_connection1->socket().send_queue_size());
    ASSERT_EQ(0, m_connection2->socket().send_queue_size());
    ASSERT_EQ(Connection::MAX_SEND_QUEUE_SIZE, m_connection1->socket().max_send_queue_size());
    ASSERT_EQ(Connection::MAX_SEND_QUEUE_SIZE, m_connection2->socket().max_send_queue_size());



    delete[] msg->data;
    delete[] data;
}

TEST_P(SocketTest, send_other_way)
{
    const uint32_t len = 4313;
    uint8_t data[len];

    uint8_t *to_send = new uint8_t[len];
    memcpy(to_send, data, len);
    m_connection1->send(to_send, len);

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
    uint8_t val1 = 12;
    uint8_t val2 = 42;

    auto type1 = new uint8_t(val1);
    auto type2 = new uint8_t(val2);
    
    const uint32_t len = sizeof(uint8_t);

    m_connection2->send(type1, len);
    m_connection2->send(type2, len);

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

    ASSERT_EQ(val1, *msg1->data);
    ASSERT_EQ(len, msg1->length);
    ASSERT_EQ(val2, *msg2->data);
    ASSERT_EQ(len, msg2->length);

    delete[] msg1->data;
    delete[] msg2->data;
}

INSTANTIATE_TEST_CASE_P(SocketTests, SocketTest,
        testing::Values(ProtocolType::TCP, ProtocolType::TLS));
