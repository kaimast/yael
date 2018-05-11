#include "yael/DelayedNetworkSocketListener.h"
#include "yael/EventLoop.h"

namespace yael
{

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

void DelayedNetworkSocketListener::set_socket(std::unique_ptr<network::Socket> &&socket, SocketType type)
{
    if(m_delay > 0)
    {
        auto &el = EventLoop::get_instance();
        m_sender = el.make_event_listener<MessageSender>(*socket);

        DLOG(INFO) << "Created delayed socket with " << m_delay << " ms delay";
    }
        
    NetworkSocketListener::set_socket(std::move(socket), type);
}

void DelayedNetworkSocketListener::MessageSender::schedule(const uint8_t *data, size_t length, uint32_t delay)
{
    auto copy = new uint8_t[length];
    memcpy(copy, data, length);
    m_pending_messages.emplace_back(std::pair{copy, length});

    auto &el = EventLoop::get_instance();
    el.register_time_event(delay, shared_from_this());
}



}
