#pragma once

#include <unordered_map>
#include <mutex>
#include <stdint.h>

namespace yael
{

class SocketListener;

class EventLoop
{
public:
    EventLoop();
    ~EventLoop();

    void run();

    void register_socket_listener(int32_t fileno, SocketListener *listener);

    void stop();

private:
    void update();

private:
    bool m_okay;

    std::mutex m_epoll_mutex;
    std::mutex m_socket_listeners_mutex;

    std::unordered_map<int32_t, SocketListener*> m_socket_listeners;

    int32_t m_epoll_fd;
};

}


