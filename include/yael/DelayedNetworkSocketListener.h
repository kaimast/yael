#pragma once

#include <cstring>

#include <list>
#include <tuple>

#include "NetworkSocketListener.h"

namespace yael
{

// Like NetworkSocketListener but adds a delay
// before sending data.
class DelayedNetworkSocketListener
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

            m_socket.send(data, length);
            delete []data;
            m_pending_messages.pop_front();
        }

        void schedule(const uint8_t *data, size_t length, uint32_t delay)
        {   
            auto copy = new uint8_t[length];
            memcpy(copy, data, length);
            m_pending_messages.emplace_back(std::pair{copy, length});
        }

    private:
        std::list<std::pair<uint8_t*, size_t>> m_pending_messages;
        network::Socket &m_socket;
    };

public:
    DelayedNetworkSocketListener(uint32_t delay)
        : m_delay(delay)
    {}

    DelayedNetworkSocketListener(uint32_t delay, std::unique_ptr<network::Socket> &&socket, SocketType type);

    bool send(const uint8_t *data, size_t length)
    {
        m_sender->schedule(data, length, m_delay);
        return true;
    }

private:
    std::shared_ptr<MessageSender> m_sender;
    uint32_t m_delay;
};

}
