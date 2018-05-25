#include "yael/DelayedNetworkSocketListener.h"
#include "yael/EventLoop.h"
#include "yael/TimeEventListener.h"

namespace yael
{

class DelayedMessageSender: public TimeEventListener
{
public:
    DelayedMessageSender(network::Socket &socket)
        : m_socket(socket)
    {}

    void on_time_event() override
    {
        if(!m_socket.is_valid())
        {
            return;
        }
        
        auto it = m_pending_messages.front();
        auto &[data, length] = it;
        bool res;

        try {
            res = m_socket.send(data, length);
            delete []data;
        } catch(network::socket_error &e) {
            res = false;
        }

        // FIXME not sure how to pass the result back to the app
        (void)res;

        m_pending_messages.pop_front();
    }

    void schedule(const uint8_t *data, size_t length, uint32_t delay)
    {
        auto copy = new uint8_t[length];
        memcpy(copy, data, length);
        m_pending_messages.emplace_back(std::pair{copy, length});
        
        TimeEventListener::schedule(delay);
    }

private:
    std::list<std::pair<uint8_t*, size_t>> m_pending_messages;

    network::Socket &m_socket;
};


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
    if(EventLoop::is_initialized())
    {
        auto &el = EventLoop::get_instance();
        el.unregister_event_listener(m_sender, true);
    }
}

bool DelayedNetworkSocketListener::send(const uint8_t *data, size_t length)
{
    if(m_delay == 0)
    {
        // default behaviour if no artificial delay specified
        return NetworkSocketListener::send(data, length);
    }

    m_sender->lock();
    m_sender->schedule(data, length, m_delay);
    m_sender->unlock();

    return true;
}

void DelayedNetworkSocketListener::set_socket(std::unique_ptr<network::Socket> &&socket, SocketType type)
{
    if(m_delay > 0)
    {
        auto &el = EventLoop::get_instance();
        m_sender = el.make_event_listener<DelayedMessageSender>(*socket);

        DLOG(INFO) << "Created delayed socket with " << m_delay << " ms delay";
    }
        
    NetworkSocketListener::set_socket(std::move(socket), type);
}


}
