#include <thread>
#include <gtest/gtest.h>
#include <yael/network/Socket.h>

using namespace yael::network;

const uint32_t data_len = 50 * 1000 * 1000;
const Address addr = resolve_URL("localhost", 62123);

static uint32_t receive_len = 0;
static uint8_t* data_received = nullptr;

static void receive_data()
{
    Socket socket;

    bool connected = socket.connect(addr);
    ASSERT_TRUE(connected);

    std::optional<Socket::message_in_t> msg;

    while(!msg)
    {
        msg = socket.receive();
    }
    
    ASSERT_TRUE(socket.is_connected());

    data_received = msg->data;
    receive_len = msg->length;
}

/**
 * Note that this test will busy wait
 * In a real application we would use the event loop to notify us when a socket has data
 */
class SocketAsyncTest : public testing::Test
{
};

TEST(SocketAsyncTest, send_large_chunk)
{
    Socket socket;

    bool listening = socket.listen(addr.IP, addr.PortNumber, 10);
    ASSERT_TRUE(listening);

    std::thread thread(receive_data);

    Socket *peer_socket = nullptr;
   
    while(peer_socket == nullptr)
    {
       auto socks = socket.accept();

       if(socks.size() > 0)
       {
           peer_socket = socks[0];
       }
    }

    uint8_t *data = new uint8_t[data_len];

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


