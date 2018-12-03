#pragma once

#include <list>
#include <vector>
#include <cstdint>
#include <cstring>
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
    static constexpr size_t DEFAULT_MAX_SEND_QUEUE_SIZE = 1024 * 1024;

    TcpSocket(const Socket& other) = delete;

    TcpSocket(size_t max_send_queue_size= DEFAULT_MAX_SEND_QUEUE_SIZE);
    virtual ~TcpSocket();

    std::vector<std::unique_ptr<Socket>> accept() override;

    bool has_messages() const override;
    bool connect(const Address& address, const std::string& name = "") override __attribute__((warn_unused_result));

    bool listen(const Address& address, uint32_t backlog) override __attribute__((warn_unused_result));

    using Socket::listen;

    /// Returns true if the socket closed right away
    /// False if there is still data to be written
    virtual bool close(bool fast = false) override;

    inline bool wait_connection_established() override
    {
        return is_connected();
    }

    bool send(const uint8_t *data, uint32_t len) override __attribute__((warn_unused_result));
    bool send(std::unique_ptr<uint8_t[]> &&data, uint32_t len) override __attribute__((warn_unused_result));

    bool do_send() override __attribute__((warn_unused_result));

    uint16_t port() const override;

    virtual bool is_connected() const override;

    bool is_listening() const override;

    const Address& get_remote_address() const override;

    int32_t get_fileno() const override;

    std::optional<message_in_t> receive() override;

    bool is_valid() const override { return m_fd > 0; }

    size_t max_send_queue_size() const override { return m_max_send_queue_size; }

protected:
    struct message_out_internal_t
    {
        message_out_internal_t(std::unique_ptr<uint8_t[]> &&data, uint32_t length)
            : data(std::move(data)), length(length), sent_pos(0)
        {
        }

        message_out_internal_t(message_out_internal_t &&other)
            : data(std::move(other.data)), length(other.length), sent_pos(other.sent_pos)
        {
            other.length = other.sent_pos = 0;
        }

        void operator=(message_out_internal_t &&other)
        {
            data = std::move(other.data);
            length = other.length;
            sent_pos = other.sent_pos;

            other.length = other.sent_pos = 0;
        }

        std::unique_ptr<uint8_t[]> data;
        msg_len_t length;
        msg_len_t sent_pos; 
    };
        
    //! Construct as a child socket
    //! Is only called by Socket::accept
    TcpSocket(int fd, size_t max_send_queue_size);

    //! Pull new messages from the socket onto our stack
    virtual void pull_messages();

    int32_t internal_accept();
    
    bool create_fd();

    //! Take a message from the stack (if any)
    bool get_message(message_in_t& message);

    //! Receive data from the socket
    //! Only used by pull_messages
    bool receive_data(buffer_t &buffer);

    //! (Re)calculate m_remote_address
    //! Called by constructor and connect()
    void calculate_remote_address();

    void update_port_number();

    //! Initialize and bind the socket
    //! Only used by connect() and listen()
    bool bind_socket(const Address& address);

    //! Port used on our side of the connection
    uint16_t m_port;
    bool m_is_ipv6;

    //! File descriptor
    int m_fd;

    //! The address of the connected client (if any)
    //! Will still be valid after close() was called
    //! Also used to register with the parent socket
    Address m_remote_address;

    std::unique_ptr<MessageSlicer> m_slicer;

private:
    enum class State
    {
        Listening,
        Connected,
        Shutdown,
        Closed,
        Unknown
    };

    std::mutex m_send_mutex;
    std::vector<message_out_internal_t> m_send_queue;

    State m_state = State::Unknown;

    // Queue at most 1Mb
    const size_t m_max_send_queue_size = 1024 * 1024;
    size_t m_send_queue_size = 0;
};

inline int32_t TcpSocket::get_fileno() const
{
    return m_fd;
}

inline bool TcpSocket::is_connected() const
{
    return m_state == State::Connected;
}

inline bool TcpSocket::is_listening() const
{
    return m_state == State::Listening;
}

inline bool TcpSocket::send(const uint8_t *data, uint32_t len)
{
    auto cpy = std::make_unique<uint8_t[]>(len); 
    memcpy(cpy.get(), data, len);

    return send(std::move(cpy), len);
}

}
}
