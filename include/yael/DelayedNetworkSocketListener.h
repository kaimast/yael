#pragma once

#include <cstring>

#include <list>
#include <tuple>

#include "NetworkSocketListener.h"

namespace yael
{

// Like NetworkSocketListener but adds a delay
// before sending data.
class DelayedNetworkSocketListener : public NetworkSocketListener
{
private:
    class MessageSender: public EventListener
    {
    public:
        MessageSender(network::Socket &socket)
            : m_socket(socket)
        {}

        void update() override
        {
            auto it = m_pending_messages.front();
            auto &[data, length] = it;

            bool res = m_socket.send(data, length);
            delete []data;
            m_pending_messages.pop_front();

            // FIXME not sure how to pass the result back to the app
            (void)res;
        }

        void schedule(const uint8_t *data, size_t length, uint32_t delay);

    private:
        std::list<std::pair<uint8_t*, size_t>> m_pending_messages;
        network::Socket &m_socket;
    };

public:
    DelayedNetworkSocketListener(uint32_t delay);

    DelayedNetworkSocketListener(uint32_t delay, std::unique_ptr<network::Socket> &&socket, SocketType type);

    bool send(const uint8_t *data, size_t length)
    {
        if(m_delay == 0)
        {
            // default behaviour if no artificial delay specified
            return NetworkSocketListener::send(data, length);
        }

        m_sender->schedule(data, length, m_delay);
        return true;
    }

protected:
    virtual void set_socket(std::unique_ptr<network::Socket> &&socket, SocketType type) override;

private:
    std::shared_ptr<MessageSender> m_sender;
    const uint32_t m_delay;
};

}
