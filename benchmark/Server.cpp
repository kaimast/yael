#include "yael.h"
#include "defines.h"

#include <chrono>
#include <thread>
#include <assert.h>
#include <iostream>

using namespace yael;

class ClientHandler : protected SocketListener
{
public:
    ClientHandler(network::Socket *socket)
        : SocketListener(socket)
    {
    }

protected:
    void update() override
    {
        char *data = nullptr;
        uint32_t len = 0;

        bool res = socket().receive(data, len);

        if(!res)
            return; // disconnected;

        assert(len == sizeof(uint8_t));
        uint8_t msg = reinterpret_cast<uint8_t*>(data)[0];

        if(msg == MSG_TYPE_PING)
        {
            LOG(INFO) << "Recieved ping!";

            std::chrono::milliseconds dur(SERVER_DELAY);
            std::this_thread::sleep_for(dur);

            send_pong();
        }
        else
            throw std::runtime_error("Received unknown data");
    }

private:
    void send_pong()
    {
        bool res = socket().send(reinterpret_cast<const char*>(&MSG_TYPE_PONG), sizeof(MSG_TYPE_PONG));

        if(!res)
            throw std::runtime_error("Failed to send data!");
    }

    uint32_t m_pong_count = 0;
};

class Server : protected SocketListener
{
public:
    Server() : SocketListener()
    {
    }

    ~Server()
    {
        EventLoop::destroy();
    }

    bool init()
    {
        EventLoop::initialize();

        auto socket = new network::Socket();

        bool res = socket->listen("localhost", BENCHMARK_PORT, 100);

        if(!res)
        {
            std::cerr << "Failed to bind port" << std::endl;
            return false;
        }

        socket->set_blocking(false);

        SocketListener::set_socket(socket);
        return true;
    }

    /**
     * This function runs the even loop until the server is terminated.
     */
    void run()
    {
        auto &loop = EventLoop::get_instance();
        loop.run();
        loop.wait();
    }

protected:
    void update() override
    {
        auto s = socket().accept();
        new ClientHandler(s);
    }
};

int main()
{
    Server server;

    if(!server.init())
        return -1;

    server.run();
    return 0;
}
