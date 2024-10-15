#include <gtest/gtest.h>
#include <yael/EventLoop.h>
#include <yael/NetworkSocketListener.h>
#include <yael/network/TcpSocket.h>
#include <yael/network/TlsSocket.h>

#include <list>
#include <optional>

using namespace yael;
using namespace yael::network;

class Connection : public yael::NetworkSocketListener {
  public:
    static constexpr size_t MAX_SEND_QUEUE_SIZE = 10 * 1024 * 1024;

    Connection(const Address &addr, ProtocolType type) {
        Socket *socket;

        if (type == ProtocolType::TCP) {
            socket = new TcpSocket(MessageMode::Datagram, MAX_SEND_QUEUE_SIZE);
        } else {
            socket = new TlsSocket(MessageMode::Datagram, "", "",
                                   MAX_SEND_QUEUE_SIZE);
        }

        if (!socket->connect(addr)) {
            throw std::runtime_error("Connection failed");
        }

        NetworkSocketListener::set_socket(std::unique_ptr<Socket>(socket),
                                          SocketType::Connection);
    }

    explicit Connection() = default;

    Connection(const Connection &other) = delete;

    using NetworkSocketListener::set_socket;

    std::optional<message_in_t> receive() {
        const std::unique_lock lock(m_mutex);
        std::optional<message_in_t> out = {};

        if (!m_messages.empty()) {
            out = m_messages.front();
            m_messages.pop_front();
        }

        return out;
    }

    void on_network_message(message_in_t &msg) override {
        const std::unique_lock lock(m_mutex);
        m_messages.push_back(msg);
    }

  private:
    std::mutex m_mutex;
    std::list<message_in_t> m_messages;
};

class Server : public yael::NetworkSocketListener {
  public:
    Server(const Address &addr, std::shared_ptr<Connection> &conn,
           ProtocolType type)
        : m_connection(conn) {
        Socket *socket;

        if (type == ProtocolType::TCP) {
            socket = new TcpSocket(MessageMode::Datagram,
                                   Connection::MAX_SEND_QUEUE_SIZE);
        } else {
            socket = new TlsSocket(MessageMode::Datagram, "../test/test.key",
                                   "../test/test.cert",
                                   Connection::MAX_SEND_QUEUE_SIZE);
        }

        if (!socket->listen(addr, 10)) {
            throw std::runtime_error("Cannot start server: listen failed");
        }

        NetworkSocketListener::set_socket(std::unique_ptr<Socket>(socket),
                                          SocketType::Acceptor);
    }

    void on_new_connection(std::unique_ptr<Socket> &&socket) override {
        m_connection->set_socket(std::move(socket), SocketType::Connection);

        auto &el = EventLoop::get_instance();
        el.register_event_listener(m_connection);
    }

  private:
    std::shared_ptr<Connection> m_connection;
};

class AsyncSocketTest : public testing::TestWithParam<ProtocolType> {
  protected:
    static constexpr uint16_t PORT = 62123;

    /// For all tests we just connect two TCP sockets to each other
    void SetUp() override {
        EventLoop::initialize();
        auto &el = EventLoop::get_instance();

        const Address addr = resolve_URL("localhost", PORT);

        m_connection1 = el.allocate_event_listener<Connection>();
        m_server =
            el.make_event_listener<Server>(addr, m_connection1, GetParam());

        m_connection2 = el.make_event_listener<Connection>(addr, GetParam());

        m_connection1->wait_for_connection();
        m_connection2->wait_for_connection();
    }

    void TearDown() override {
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

    std::shared_ptr<Server> m_server = nullptr;
    std::shared_ptr<Connection> m_connection1 = nullptr;
    std::shared_ptr<Connection> m_connection2 = nullptr;
};

TEST_P(AsyncSocketTest, remote_address) {
    EXPECT_EQ(PORT, m_connection1->socket().port());
    EXPECT_EQ(PORT, m_connection2->socket().get_remote_address().PortNumber);
}

TEST_P(AsyncSocketTest, send_one_way) {
    const uint32_t len = 4313;
    auto data = new uint8_t[len];

    m_connection2->send(data, len, false, true);

    std::optional<message_in_t> msg;

    while (!msg) {
        msg = m_connection1->receive();
    }

    ASSERT_EQ(len, msg->length);
    ASSERT_EQ(0, memcmp(data, msg->data, len));

    delete[] msg->data;
}

TEST_P(AsyncSocketTest, send_large_chunk) {
    const uint32_t len = 50 * 1000 * 1000;
    auto *data = new uint8_t[len];

    auto to_send = new uint8_t[len];
    memcpy(to_send, data, len);

    m_connection2->send(to_send, len, false, true);

    std::optional<message_in_t> msg;

    while (!msg) {
        msg = m_connection1->receive();
    }

    ASSERT_EQ(len, msg->length);
    ASSERT_EQ(0, memcmp(data, msg->data, len));

    ASSERT_EQ(0U, m_connection1->socket().send_queue_size());
    ASSERT_EQ(0U, m_connection2->socket().send_queue_size());
    ASSERT_EQ(Connection::MAX_SEND_QUEUE_SIZE,
              m_connection1->socket().max_send_queue_size());
    ASSERT_EQ(Connection::MAX_SEND_QUEUE_SIZE,
              m_connection2->socket().max_send_queue_size());

    delete[] msg->data;
    delete[] data;
}

TEST_P(AsyncSocketTest, send_other_way) {
    const uint32_t len = 4313;
    uint8_t data[len];

    auto *to_send = new uint8_t[len];
    memcpy(to_send, data, len);
    m_connection1->send(to_send, len, false, true);

    std::optional<message_in_t> msg;

    while (!msg) {
        msg = m_connection2->receive();
    }

    ASSERT_EQ(len, msg->length);
    ASSERT_EQ(0, memcmp(data, msg->data, len));

    delete[] msg->data;
}

INSTANTIATE_TEST_CASE_P(AsyncSocketTests, AsyncSocketTest,
                        testing::Values(ProtocolType::TCP, ProtocolType::TLS));
