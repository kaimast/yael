#include "yael.h"
#include "defines.h"

#include <chrono>
#include <thread>
#include <assert.h>
#include <iostream>

using namespace yael;

class ClientHandler : protected NetworkSocketListener
{
public:
    ClientHandler(network::Socket *socket)
        : NetworkSocketListener(socket)
    {
        LOG(INFO) << "Client connected";
    }

    ~ClientHandler()
    {
        LOG(INFO) << "Client disconnected";
    }

protected:
    void update() override
    {
        uint8_t *data = nullptr;
        uint32_t len = 0;

        bool res = socket().receive(data, len);

        if(!res)
            return; // disconnected;

        assert(len == sizeof(uint8_t));
        uint8_t msg = reinterpret_cast<uint8_t*>(data)[0];

        if(msg == MSG_TYPE_PING)
        {
            DLOG(INFO) << "Recieved ping!";

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
        bool res = socket().send(reinterpret_cast<const uint8_t*>(&MSG_TYPE_PONG), sizeof(MSG_TYPE_PONG));

        if(!res)
        {
            throw std::runtime_error("Failed to send data!");
        }
        else
            DLOG(INFO) << "Sent pong";
    }

    uint32_t m_pong_count = 0;
};

class Server : protected NetworkSocketListener
{
public:
    Server() : NetworkSocketListener()
    {
    }

    ~Server()
    {
        LOG(INFO) << "Shutting down.";
        EventLoop::destroy();
    }

    bool init(const std::string& name)
    {
        EventLoop::initialize();

        auto socket = new network::Socket();

        bool res = socket->listen(name, BENCHMARK_PORT, 100);

        if(!res)
        {
            std::cerr << "Failed to bind port" << std::endl;
            return false;
        }

        socket->set_blocking(false);
        NetworkSocketListener::set_socket(socket);

        LOG(INFO) << "Server initialized";
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

int main(int argc, char* argv[])
{
    Server server;

    if(argc < 2)
    {
        std::cerr << "No name to listen on supplied!" << std::endl;
        return -1;
    }

    if(!server.init(argv[1]))
        return -1;

    server.run();
    return 0;
}
