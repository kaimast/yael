#include "yael/DelayedNetworkSocketListener.h"
#include "yael/EventLoop.h"
#include "yael/TimeEventListener.h"

namespace yael
{

class DelayedMessageSender: public TimeEventListener
{
public:
    DelayedMessageSender(network::Socket *socket)
        : m_socket(socket)
    {}

    void on_time_event() override
    {
        if(m_socket == nullptr || !m_socket->is_valid())
        {
            return;
        }

        auto &socket = *m_socket;
        
        auto it = m_pending_messages.begin();
        auto &[data, length] = *it;
        bool res;

        try {
            res = socket.send(std::move(data), length);
        } catch(network::socket_error &e) {
            res = false;
        }

        // FIXME not sure how to pass the result back to the app
        (void)res;

        m_pending_messages.erase(it);
    }

    void close()
    {
        m_socket = nullptr;
        TimeEventListener::close();
    }

    void schedule(std::unique_ptr<uint8_t[]> &&data, size_t length, uint32_t delay)
    {
        m_pending_messages.emplace_back(std::pair{std::move(data), length});
        TimeEventListener::schedule(delay);
    }

    void schedule(const uint8_t *data, size_t length, uint32_t delay)
    {
        auto copy = std::make_unique<uint8_t[]>(length);
        memcpy(copy.get(), data, length);
        m_pending_messages.emplace_back(std::pair{std::move(copy), length});
        TimeEventListener::schedule(delay);
    }

private:
    std::list<std::pair<std::unique_ptr<uint8_t[]>, size_t>> m_pending_messages;
    network::Socket *m_socket;
};

DelayedNetworkSocketListener::DelayedNetworkSocketListener()
    : m_delay(0)
{
}

DelayedNetworkSocketListener::DelayedNetworkSocketListener(uint32_t delay)
    : m_delay(delay)
{
}

DelayedNetworkSocketListener::DelayedNetworkSocketListener(uint32_t delay, std::unique_ptr<network::Socket> &&socket, SocketType type)
    : m_delay(delay)
{
    if(socket)
    {
        DelayedNetworkSocketListener::set_socket(std::forward<std::unique_ptr<network::Socket>>(socket), type);
    }
}

DelayedNetworkSocketListener::~DelayedNetworkSocketListener()
{
    if(EventLoop::is_initialized() && m_sender != nullptr)
    {
        m_sender->lock();
        m_sender->close();
        m_sender->unlock();
    }
}

void DelayedNetworkSocketListener::send(std::unique_ptr<uint8_t[]> &&data, size_t length)
{
    if(m_delay == 0)
    {
        // default behaviour if no artificial delay specified
        return NetworkSocketListener::send(std::move(data), length);
    }

    m_sender->lock();
    m_sender->schedule(std::move(data), length, m_delay);
    m_sender->unlock();
}

void DelayedNetworkSocketListener::send(const uint8_t *data, size_t length)
{
    if(m_delay == 0)
    {
        // default behaviour if no artificial delay specified
        return NetworkSocketListener::send(data, length);
    }

    m_sender->lock();
    m_sender->schedule(data, length, m_delay);
    m_sender->unlock();
}

void DelayedNetworkSocketListener::set_delay(uint32_t delay)
{
    m_delay = delay;
}

void DelayedNetworkSocketListener::set_socket(std::unique_ptr<network::Socket> &&socket, SocketType type)
{
    auto &el = EventLoop::get_instance();
    m_sender = el.make_event_listener<DelayedMessageSender>(socket.get());
    NetworkSocketListener::set_socket(std::move(socket), type);
}


}
