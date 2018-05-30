#include "yael/NetworkSocketListener.h"
#include "yael/EventLoop.h"

using namespace yael;

NetworkSocketListener::NetworkSocketListener()
    : m_socket(nullptr), m_fileno(-1)
{
}

NetworkSocketListener::NetworkSocketListener(std::unique_ptr<network::Socket> &&socket, SocketType type)
    : m_socket(nullptr), m_socket_type(SocketType::None), m_fileno(-1)
{
    if(socket)
    {
        NetworkSocketListener::set_socket(std::forward<std::unique_ptr<network::Socket>>(socket), type);

        m_socket->wait_connection_established();
    }
}

std::unique_ptr<network::Socket> NetworkSocketListener::release_socket()
{
    // Move socket before we unregistered so socket doesn't get closed
    auto sock = std::move(m_socket);

    auto &el = EventLoop::get_instance();
    el.unregister_event_listener(std::dynamic_pointer_cast<EventListener>(shared_from_this()));

    return sock;
}

void NetworkSocketListener::set_socket(std::unique_ptr<network::Socket> &&socket, SocketType type)
{
    if(m_socket)
    {
        throw std::runtime_error("There is already a socket assigned to this listener!");
    }

    if(!socket->is_valid())
    {
        throw std::runtime_error("Not a valid socket!");
    }

    m_socket = std::move(socket);
    m_socket_type = type;
    m_fileno = m_socket->get_fileno();
}

bool NetworkSocketListener::is_valid() const
{ 
    if(!m_socket)
    {
        return false;
    }

    return m_socket->is_connected();
}

void NetworkSocketListener::update()
{
    switch(m_socket_type)
    {
    case SocketType::Acceptor:
    {
        for(auto s: m_socket->accept())
        {
            this->on_new_connection(std::unique_ptr<network::Socket>{s});
        }

        break;
    }
    case SocketType::Connection:
    {
        try
        {
            if(m_socket)
            {
                while(true)
                {
                    auto message = m_socket->receive();

                    if(message)
                    {
                        this->on_network_message(*message);
                    }
                    else
                    {
                        // no more data
                        break;
                    }
                }
            }
        }
        catch (const network::socket_error &e)
        {
            LOG(WARNING) << e.what();
        }

        // After processing the last message we will notify the user that the socket is closed
        if(m_socket && !m_socket->is_valid())
        {
            close_socket();
        }
 
        break;
    }
    default:
        throw std::runtime_error("Unknown socket type!");
    }

}

void NetworkSocketListener::close_socket()
{
    if(!m_has_disconnected)
    {
        m_has_disconnected = true;
        m_socket->close();
        
        this->on_disconnect();

        // Don't unregister during event loop shutdown
        if(EventLoop::is_initialized())
        {
            auto &el = EventLoop::get_instance();
            el.unregister_event_listener(std::dynamic_pointer_cast<EventListener>(shared_from_this()));
        }
    }
}

int32_t NetworkSocketListener::get_fileno() const
{
    return m_fileno;
}
