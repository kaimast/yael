#pragma once

#include <mutex>
#include <memory>
#include <glog/logging.h>

#include "network/Socket.h"
#include "EventListener.h"

namespace yael
{

enum class SocketType { None, Acceptor, Connection };

class NetworkSocketListener: public EventListener
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

    NetworkSocketListener(const EventListener& other) = delete;
    virtual ~NetworkSocketListener() = default;

    /**
     * @brief get the current or most recent fileno associated with this listener
     * @return -1 if there was never a valid Socket associated with this listener
     */
    int32_t get_fileno() const override;

    /**
     * @brief returns true if the associated socket exists (either connected, connecting, shutting down, or listening)
     */
    bool is_valid() override;

    bool is_connected();

    void wait_for_connection();

    /// Callbacks
    virtual void on_network_message(network::Socket::message_in_t &msg) { (void)msg; }
    virtual void on_new_connection(std::unique_ptr<network::Socket> &&socket) { (void)socket; }
    virtual void on_disconnect() {}

    bool has_messages()
    {
        std::unique_lock lock(m_mutex);
        return m_socket && m_socket->has_messages();
    }

    void send(std::unique_ptr<uint8_t[]> &&data, size_t length, bool blocking = false, bool async = false);
    void send(const uint8_t *data, size_t length, bool blocking = false, bool async = false);

    virtual void close_socket() override;

    const network::Socket& socket() const
    {
        return *m_socket;
    }

    EventListener::Mode mode() override
    {
        // send queue decides the mode of the listener
        std::unique_lock lock(m_send_mutex);
        return m_mode;
    }

protected:
    /**
     * @brief Hand a valid socket to the listener
     * @param socket the socket
     * @throw std::runtime_error if there is already a socket assigned to this listener
     */
    virtual void set_socket(std::unique_ptr<network::Socket> &&socket, SocketType type);

    /// Hand over socket to another object
    /// Note: this will unregister the socket listener but *not* close the socket
    std::unique_ptr<network::Socket> release_socket();

private:
    void close_socket_internal(std::unique_lock<std::mutex> &lock);

    void set_mode(EventListener::Mode mode);

    void on_read_ready() override final;
    void on_write_ready() override final;
    void on_error() override final;

    std::mutex m_mutex;

    // Sends might update the mode (Read or ReadWrite) of the socket
    // This needs to happen atomically
    std::mutex m_send_mutex;

    std::unique_ptr<network::Socket> m_socket = nullptr;
    SocketType m_socket_type = SocketType::None;
    int32_t m_fileno = 1;
    
    bool m_has_disconnected = false;

    EventListener::Mode m_mode = EventListener::Mode::ReadOnly;
};

inline void NetworkSocketListener::close_socket()
{
    std::unique_lock lock(m_mutex);
    close_socket_internal(lock);
}

}

