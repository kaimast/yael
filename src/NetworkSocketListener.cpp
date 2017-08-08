#include "yael/NetworkSocketListener.h"
#include "yael/EventLoop.h"

using namespace yael;

NetworkSocketListener::NetworkSocketListener()
    : m_socket(nullptr), m_fileno(-1)
{
}

NetworkSocketListener::NetworkSocketListener(network::Socket *socket)
    : m_socket(nullptr), m_fileno(-1)
{
    if(socket)
        set_socket(socket);
}

NetworkSocketListener::NetworkSocketListener(std::unique_ptr<network::Socket> &&socket)
    : m_socket(nullptr), m_fileno(-1)
{
    set_socket(std::move(socket));
}

NetworkSocketListener::~NetworkSocketListener()
{
}

void NetworkSocketListener::set_socket(std::unique_ptr<network::Socket> &&socket) throw(std::runtime_error)
{
    if(m_socket)
        throw std::runtime_error("There is already a socket assigned to this listener!");

    if(!socket->is_valid())
        throw std::runtime_error("Not a valid socket!");

    m_socket = std::move(socket);
    m_fileno = m_socket->get_fileno();
}

bool NetworkSocketListener::is_valid() const
{
    if(!m_socket)
        return false;

    return m_socket->is_valid();
}

int32_t NetworkSocketListener::get_fileno() const
{
    return m_fileno;
}
