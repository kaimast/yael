#pragma once

#include <mutex>
#include <memory>
#include <glog/logging.h>

#include "network/Socket.h"
#include "SocketListener.h"

namespace yael
{

enum class SocketType { None, Acceptor, Connection };

class NetworkSocketListener: public SocketListener
{
public:
    /**
     * @brief Construct with a valid socket
     */
    NetworkSocketListener(std::unique_ptr<network::Socket> &&socket, SocketType type);

    /**
     * @brief Construct without a valid socket (yet)
     */
    NetworkSocketListener();

    NetworkSocketListener(const SocketListener& other) = delete;
    virtual ~NetworkSocketListener() = default;

    /**
     * @brief Hand a valid socket to the listener
     * @param socket the socket
     * @throw std::runtime_error if there is already a socket assigned to this listener
     */
    void set_socket(std::unique_ptr<network::Socket> &&socket, SocketType type);

    /**
     * @brief get the current or most recent fileno associated with this listener
     * @return -1 if there was never a valid Socket associated with this listener
     */
    int32_t get_fileno() const override;

    /**
     * @brief returns true if the associated socket is connected
     */
    bool is_valid() const override;

    /// Callbacks
    virtual void on_network_message(network::Socket::message_in_t &msg) { (void)msg; }
    virtual void on_new_connection(std::unique_ptr<network::Socket> &&socket) { (void)socket; }
    virtual void on_disconnect() {}

    bool has_messages() const override
    {
        return is_valid() && m_socket->has_messages();
    }

protected:
    void update() override;

    network::Socket& socket()
    {
        return *m_socket;
    }

    const network::Socket& socket() const
    {
        return *m_socket;
    }

private:
    void close_hook();

    std::unique_ptr<network::Socket> m_socket;
    SocketType m_socket_type;
    int32_t m_fileno;
};

}

