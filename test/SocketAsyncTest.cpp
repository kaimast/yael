#include "SocketAsyncTest.h"
#include <thread>

using namespace yael::network;

const uint32_t data_len = 50 * 1000 * 1000;
const Address addr = resolveUrl("localhost", 62123);

static uint32_t receive_len = 0;
static char* data_received = nullptr;

static void receive_data()
{
    Socket socket;

    bool connected = socket.connect(addr);
    ASSERT_TRUE(connected);

    bool has_data = socket.receive(data_received, receive_len, true);

    ASSERT_TRUE(has_data);
    ASSERT_TRUE(socket.is_connected());
}

TEST(SocketAsyncTest, send_large_chunk)
{
    Socket socket;

    bool listening = socket.listen(addr.IP, addr.PortNumber, 10);
    ASSERT_TRUE(listening);

    std::thread thread(receive_data);

    Socket *peer_socket = socket.accept();

    char *data = new char[data_len];

    bool sent = peer_socket->send(data, data_len);
    ASSERT_TRUE(sent);

    thread.join();

    ASSERT_EQ(data_len, receive_len);
    ASSERT_EQ(0, memcmp(data, data_received, data_len));

    ASSERT_TRUE(peer_socket->is_connected());

    delete []data_received;
    delete []data;

    delete peer_socket;
}


