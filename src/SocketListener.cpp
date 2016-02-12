#include "SocketListener.h"
#include "EventLoop.h"

using namespace yael;

SocketListener::SocketListener()
    : m_socket(nullptr), m_fileno(-1), m_last_update()
{
}

SocketListener::SocketListener(network::Socket *socket)
    : m_socket(nullptr), m_fileno(-1), m_last_update()
{
    set_socket(socket);
}

SocketListener::SocketListener(std::unique_ptr<network::Socket> &&socket)
    : m_socket(nullptr), m_fileno(-1), m_last_update()
{
    set_socket(std::move(socket));
}

SocketListener::~SocketListener()
{
}

void SocketListener::set_socket(std::unique_ptr<network::Socket> &&socket) throw(std::runtime_error)
{
    if(m_socket)
        throw std::runtime_error("There is already a socket assigned to this listener!");

    if(!socket->is_valid())
        throw std::runtime_error("Not a valid socket!");

    m_socket = std::move(socket);
    m_fileno = m_socket->get_fileno();

    auto &loop = EventLoop::get_instance();
    loop.register_socket_listener(m_fileno, this);
}

bool SocketListener::is_valid() const
{
    if(!m_socket)
        return false;

    return m_socket->is_valid();
}

int32_t SocketListener::get_fileno() const
{
    return m_fileno;
}
