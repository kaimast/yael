#include "yael/DelayedNetworkSocketListener.h"
#include "yael/EventLoop.h"
#include "yael/TimeEventListener.h"

namespace yael
{

class DelayedMessageSender: public TimeEventListener
{
private:
    struct delayed_message_t
    {
        std::unique_ptr<uint8_t[]> data;
        size_t length;
        bool blocking;
    };

public:
    explicit DelayedMessageSender(NetworkSocketListener *socket)
        : m_socket(socket)
    {}

    void on_time_event() override
    {
        std::unique_lock lock(m_mutex);

        auto it = m_pending_messages.begin();

        if(m_socket == nullptr || !m_socket->is_valid())
        {
            LOG(WARNING) << "Discarded delayed message because socket is closed";
        }
        else
        {
            m_socket->send(std::move(it->data), it->length);
        }

        m_pending_messages.erase(it);
    }

    void close_socket() override
    {
        m_socket = nullptr;
        TimeEventListener::close_socket();
    }

    void schedule(std::unique_ptr<uint8_t[]> &&data, size_t length, uint32_t delay, bool blocking)
    {
        std::unique_lock lock(m_mutex);

        m_pending_messages.emplace_back(delayed_message_t{std::move(data), length, blocking});
        TimeEventListener::schedule(delay);
    }

    void schedule(const uint8_t *data, size_t length, uint32_t delay, bool blocking)
    {
        std::unique_lock lock(m_mutex);

        auto copy = std::make_unique<uint8_t[]>(length);
        memcpy(copy.get(), data, length);
        m_pending_messages.emplace_back(delayed_message_t{std::move(copy), length, blocking});
        TimeEventListener::schedule(delay);
    }

private:
    std::list<delayed_message_t> m_pending_messages;

    std::mutex m_mutex;
    NetworkSocketListener *m_socket;
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
    DelayedNetworkSocketListener::close_socket();
}

void DelayedNetworkSocketListener::close_socket()
{
    if(EventLoop::is_initialized() && m_sender != nullptr)
    {
        m_sender->close_socket();
    }

    NetworkSocketListener::close_socket();
}

void DelayedNetworkSocketListener::send(std::unique_ptr<uint8_t[]> &&data, size_t length, bool blocking, bool async)
{
    if(m_delay == 0)
    {
        // default behaviour if no artificial delay specified
        return NetworkSocketListener::send(std::move(data), length, blocking, async);
    }

    // this will always be async
    m_sender->schedule(std::move(data), length, m_delay, blocking);
}

void DelayedNetworkSocketListener::send(const uint8_t *data, size_t length, bool blocking, bool async)
{
    if(m_delay == 0)
    {
        // default behaviour if no artificial delay specified
        return NetworkSocketListener::send(data, length, blocking, async);
    }

    // this will always be async
    m_sender->schedule(data, length, m_delay, blocking);
}

void DelayedNetworkSocketListener::set_delay(uint32_t delay)
{
    m_delay = delay;
}

void DelayedNetworkSocketListener::set_socket(std::unique_ptr<network::Socket> &&socket, SocketType type)
{
    auto &el = EventLoop::get_instance();
    m_sender = el.make_event_listener<DelayedMessageSender>(this);
    NetworkSocketListener::set_socket(std::move(socket), type);
}


}
