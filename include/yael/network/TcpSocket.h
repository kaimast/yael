#pragma once

#include <list>
#include <vector>
#include <stdint.h>
#include <mutex>
#include <tuple>
#include <memory>

#include "Socket.h"

namespace yael
{
namespace network
{

class MessageSlicer;

/**
 * @brief Object-oriented wrapper for a TCP socket
 * @note This implementation is *not* thread safe. Use the EventLoop for multiple threads polling messages.
 */
class TcpSocket : public Socket
{
public:
    TcpSocket(const Socket& other) = delete;

    TcpSocket();
    ~TcpSocket();

    std::vector<Socket*> accept() override;

    bool has_messages() const override;
    bool connect(const Address& address, const std::string& name = "") override __attribute__((warn_unused_result));

    bool listen(const Address& address, uint32_t backlog) override __attribute__((warn_unused_result));

    using Socket::listen;

    void close() override;

    inline bool wait_connection_established() override
    {
        return is_connected();
    }

    bool send(const message_out_t& message) override __attribute__((warn_unused_result));
    using Socket::send;

    uint16_t port() const override;

    virtual bool is_connected() const override;

    bool is_listening() const override;

    const Address& get_client_address() const override;

    int32_t get_fileno() const override;

    std::optional<message_in_t> receive() override;

    bool is_valid() const { return m_fd > 0; }

protected:

    //! Construct as a child socket
    //! Is only called by Socket::accept
    TcpSocket(int fd);

    //! Pull new messages from the socket onto our stack
    virtual void pull_messages();

    int32_t internal_accept();
    
    bool create_fd();

    //! Take a message from the stack (if any)
    bool get_message(message_in_t& message);

    //! Receive data from the socket
    //! Only used by pull_messages
    bool receive_data(buffer_t &buffer);

    //! (Re)calculate m_client_address
    //! Called by constructor and connect()
    void calculate_client_address();

    void update_port_number();

    //! Initialize and bind the socket
    //! Only used by connect() and listen()
    bool bind_socket(const Address& address);

    //! Port used on our side of the connection
    uint16_t m_port;
    bool m_is_ipv6;

    //! File descriptor
    int m_fd;

    //! Is this socket listening?
    bool m_listening;

    //! The address of the connected client (if any)
    //! Will still be valid after close() was called
    //! Also used to register with the parent socket
    Address m_client_address;

    std::unique_ptr<MessageSlicer> m_slicer;
};

inline int32_t TcpSocket::get_fileno() const
{
    return m_fd;
}

inline bool TcpSocket::is_connected() const
{
    return is_valid() && !m_listening;
}

inline bool TcpSocket::is_listening() const
{
    return is_valid() && m_listening;
}

}
}
