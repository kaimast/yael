#pragma once

#include <cstring>

#include <list>
#include <tuple>

#include "NetworkSocketListener.h"

namespace yael
{

class DelayedMessageSender;

// Like NetworkSocketListener but adds a delay
// before sending data.
class DelayedNetworkSocketListener : public NetworkSocketListener
{
public:
    DelayedNetworkSocketListener(uint32_t delay);

    DelayedNetworkSocketListener(uint32_t delay, std::unique_ptr<network::Socket> &&socket, SocketType type);

    ~DelayedNetworkSocketListener();

    bool send(const uint8_t *data, size_t length);

protected:
    virtual void set_socket(std::unique_ptr<network::Socket> &&socket, SocketType type) override;

private:
    std::shared_ptr<DelayedMessageSender> m_sender;
    const uint32_t m_delay;
};

}
