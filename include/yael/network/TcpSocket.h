#pragma once

#include <list>
#include <vector>
#include <stdint.h>
#include <mutex>
#include <tuple>

#include "Socket.h"

namespace yael
{
namespace network
{

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

    void set_close_hook(std::function<void()> func);

    //! Accept new connections
    std::vector<Socket*> accept();

    bool has_messages() const override;
    bool connect(const Address& address, const std::string& name = "") override __attribute__((warn_unused_result));

    bool listen(const Address& address, uint32_t backlog) override __attribute__((warn_unused_result));
    using Socket::listen;

    void close() override;
    bool is_valid() const override;

    bool send(const message_out_t& message) override __attribute__((warn_unused_result));
    using Socket::send;

    uint16_t port() const override;

    bool is_connected() const override;

    bool is_listening() const override;

    const Address& get_client_address() const override;

    int32_t get_fileno() const override;

    std::optional<message_in_t> receive() override;

protected:
    //! Construct as a child socket
    //! Is only called by Socket::accept
    TcpSocket(int fd);

private:
    //! Pull new messages from the socket onto our stack
    void pull_messages();

    struct internal_message_in_t
    {
        internal_message_in_t()
            : length(0), read_pos(0), data(nullptr)
        {
        }

        internal_message_in_t(internal_message_in_t &&other)
            : length(other.length), read_pos(other.read_pos), data(other.data)
        {
            other.length = other.read_pos = 0;
            other.data = nullptr;
        }

        void operator=(internal_message_in_t &&other)
        {
            length = other.length;
            read_pos = other.read_pos;
            data = other.data;

            other.length = other.read_pos = 0;
            other.data = nullptr;
        }

        bool valid() const
        {
            return data != nullptr;
        }

        msg_len_t length;
        msg_len_t read_pos;
        uint8_t  *data;
    };

    bool create_fd();

    //! Take a message from the stack (if any)
    bool get_message(message_in_t& message);

    //! Receive data from the socket
    //! Only used by pull_messages
    bool receive_data();

    //! (Re)calculate m_client_address
    //! Called by constructor and connect()
    void calculate_client_address();

    void update_port_number();

    //! Initialize and bind the socket
    //! Only used by connect() and listen()
    bool bind_socket(const Address& address);

    //! Stack of incoming messages
    //! used by pull_messages() and get_message()
    std::list<internal_message_in_t> m_messages;

    //! Internal message buffer
    static constexpr int32_t BUFFER_SIZE = 4096;
    uint8_t m_buffer[BUFFER_SIZE];

    std::function<void()> m_close_hook;

    //! Port used on our side of the connection
    uint16_t m_port;
    bool m_is_ipv6;

    //! Filedescriptor
    int m_fd;

    //! Current positin in the message buffer
    //! This is used for multiple messages in one receive call
    int32_t m_buffer_pos;
    uint32_t m_buffer_size;

    //! Is this socket listening?
    bool m_listening;

    //! The address of the connected client (if any)
    //! Will still be valid after close() was called
    //! Also used to register with the parent socket
    Address m_client_address;

    //! Message in progress to be read
    bool m_has_current_message;
    internal_message_in_t m_current_message;
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

inline bool TcpSocket::has_messages() const
{
    return m_messages.size() > 0;
}

inline bool TcpSocket::is_valid() const
{
    return m_fd >= 0;
}
}
}
