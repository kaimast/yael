#include "yael/NetworkSocketListener.h"
#include "yael/EventLoop.h"

using namespace yael;

NetworkSocketListener::NetworkSocketListener()
    : EventListener(EventListener::Mode::ReadOnly), m_socket(nullptr), m_fileno(-1)
{
}

NetworkSocketListener::NetworkSocketListener(std::unique_ptr<network::Socket> &&socket, SocketType type)
    : EventListener(EventListener::Mode::ReadOnly), m_socket(nullptr), m_socket_type(SocketType::None), m_fileno(-1)
{
    if(socket)
    {
        NetworkSocketListener::set_socket(std::forward<std::unique_ptr<network::Socket>>(socket), type);

        m_socket->wait_connection_established();
    }
}

std::unique_ptr<network::Socket> NetworkSocketListener::release_socket()
{
    std::unique_lock lock(m_mutex);

    // Move socket before we unregistered so socket doesn't get closed
    auto sock = std::move(m_socket);

    ///FIXME we should tell event listener to remap the socket
    // the current approach can cause race conditions...
    auto &el = EventLoop::get_instance();
    el.unregister_event_listener(std::dynamic_pointer_cast<EventListener>(shared_from_this()));

    return sock;
}

void NetworkSocketListener::set_socket(std::unique_ptr<network::Socket> &&socket, SocketType type)
{
    std::unique_lock lock(m_mutex);

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

bool NetworkSocketListener::is_valid()
{
    std::unique_lock lock(m_mutex);

    if(!m_socket)
    {
        return false;
    }

    return m_socket->is_valid();
}

bool NetworkSocketListener::is_connected()
{
    std::unique_lock lock(m_mutex);

    if(!m_socket)
    {
        return false;
    }

    return m_socket->is_connected();
}

void NetworkSocketListener::on_write_ready()
{
    std::unique_lock lock(m_mutex);

    bool has_more = false;

    try {
        has_more = m_socket->do_send();
    } catch(const network::socket_error &e) {
        LOG(WARNING) << e.what();
        close_socket_internal(lock);
    }

    if(!has_more)
    {
        this->set_mode(EventListener::Mode::ReadOnly);
    }
}

void NetworkSocketListener::on_read_ready()
{
    std::unique_lock lock(m_mutex);

    switch(m_socket_type)
    {
    case SocketType::Acceptor:
    {
        auto result = m_socket->accept();
        lock.unlock();

        for(auto &s: result)
        {
            this->on_new_connection(std::move(s));
        }

        break;
    }
    case SocketType::Connection:
    {
        try
        {
            while(m_socket)
            {
                auto message = m_socket->receive();

                if(message)
                {
                    lock.unlock();
                    this->on_network_message(*message);
                    lock.lock();
                }
                else
                {
                    // no more data
                    break;
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
            close_socket_internal(lock);
        }
 
        break;
    }
    default:
        throw std::runtime_error("Unknown socket type!");
    }

}

void NetworkSocketListener::close_socket_internal(std::unique_lock<std::mutex> &lock)
{
    if(m_has_disconnected)
    {
        // pass
        return;
    }

    m_has_disconnected = true;

    if(m_socket)
    {
        bool done = m_socket->close();
        lock.unlock();

        if(done)
        {
            if(m_socket_type == SocketType::Connection)
            {
                this->on_disconnect();
            }
            else if(EventLoop::is_initialized())
            {
                auto &el = EventLoop::get_instance();
                el.unregister_event_listener(shared_from_this());
            }
        }
    }
}

int32_t NetworkSocketListener::get_fileno() const
{
    return m_fileno;
}
