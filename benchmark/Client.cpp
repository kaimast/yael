#include "yael.h"
#include "defines.h"

#include <chrono>
#include <iostream>
#include <assert.h>

using namespace yael;

class Client
{
public:
    Client()
    {}

    bool init()
    {
        bool res = m_socket.connect(network::resolve_URL("localhost", BENCHMARK_PORT));
        m_socket.set_blocking(true);

        return res;
    }

    void run()
    {
        uint32_t i = 0;

        while(i < NUM_ROUND_TRIPS)
        {
            auto start = std::chrono::high_resolution_clock::now();

            send_ping();
            receive_pong();

            auto end = std::chrono::high_resolution_clock::now();

            auto dur = end - start;

            double usecs = std::chrono::duration_cast<std::chrono::microseconds>(dur).count();

            std::cout << usecs / 1000.0 << std::endl;

            ++i;
        }
    }

protected:
    void receive_pong()
    {
        char *data = nullptr;
        uint32_t len = 0;

        bool res = m_socket.receive(data, len);

        if(!res)
        {
            if(!m_socket.is_connected())
                throw std::runtime_error("Lost connection to server!");
            else
                throw std::runtime_error("Failed to receive data!");
        }

        assert(len == sizeof(uint8_t));
        uint8_t msg = reinterpret_cast<uint8_t*>(data)[0];

        if(msg != MSG_TYPE_PONG)
            throw std::runtime_error("Received invalid response!");
    }

private:
    void send_ping()
    {
        bool res = m_socket.send(reinterpret_cast<const char*>(&MSG_TYPE_PING), sizeof(MSG_TYPE_PING));

        if(!res)
            throw std::runtime_error("Failed to send data!");
    }

private:
    network::Socket m_socket;
};


int main()
{
    Client client;

    if(!client.init())
    {
        std::cerr << "Failed to connect to Server" << std::endl;
        return -1;
    }

    client.run();

    return 0;
}
