#pragma once

#include <list>
#include <vector>
#include <cstdint>
#include <cstring>
#include <mutex>
#include <condition_variable>
#include <atomic>
#include <memory>

#include "MessageSlicer.h"
#include "Socket.h"

namespace yael::network
{

/**
 * @brief Object-oriented wrapper for a TCP socket
 *
 * @note While sending is thread-safe, receiving data currently is not.
 *   This code assumes you use the EventLoop to ensure only one thread reads from the socket at a time.
 */
class TcpSocket : public Socket
{
public:
    static constexpr size_t DEFAULT_MAX_SEND_QUEUE_SIZE = static_cast<size_t>(1024 * 1024);

    TcpSocket(const Socket& other) = delete;

    explicit TcpSocket(MessageMode mode = MessageMode::Datagram, size_t max_send_queue_size= DEFAULT_MAX_SEND_QUEUE_SIZE);
    ~TcpSocket() override;

    std::vector<std::unique_ptr<Socket>> accept() override;

    [[nodiscard]]
    bool has_messages() const override;

    bool connect(const Address& address, const std::string& name = "") override __attribute__((warn_unused_result));

    bool listen(const Address& address, uint32_t backlog) override __attribute__((warn_unused_result));

    using Socket::listen;

    /// Returns true if the socket closed right away
    /// False if there is still data to be written
    bool close(bool fast = false) override;

    inline bool wait_connection_established() override
    {
        return is_connected();
    }

    bool send(const uint8_t *data, uint32_t len, bool async = false) override __attribute__((warn_unused_result));

    bool send(std::unique_ptr<uint8_t[]> &data, uint32_t len, bool async = false) override __attribute__((warn_unused_result));

    bool send(std::shared_ptr<uint8_t[]> &data, uint32_t len, bool async = false) override __attribute__((warn_unused_result));

    bool do_send() override __attribute__((warn_unused_result));

    [[nodiscard]]
    uint16_t port() const override;

    [[nodiscard]]
    bool is_connected() const override;

    [[nodiscard]]
    bool is_listening() const override;

    [[nodiscard]]
    const Address& get_remote_address() const override;

    [[nodiscard]]
    int32_t get_fileno() const override;

    std::optional<message_in_t> receive() override;

    [[nodiscard]]
    bool is_valid() const override { return m_fd > 0; }

    [[nodiscard]]
    size_t send_queue_size() const  override { return m_send_queue_size; }

    [[nodiscard]]
    size_t max_send_queue_size() const override { return m_max_send_queue_size; }

    void wait_send_queue_empty() override;

    [[nodiscard]]
    const MessageSlicer& message_slicer() const override
    {
        return *m_slicer;
    }

protected:
    struct message_out_internal_t
    {
        message_out_internal_t(std::unique_ptr<uint8_t[]> data, uint32_t length_)
            : length(length_), m_is_shared(false), m_data_unique(std::move(data))
        {
        }

        message_out_internal_t(std::shared_ptr<uint8_t[]> data, uint32_t length_)
           : length(length_), m_is_shared(true), m_data_shared(std::move(data))
        {
        }

        message_out_internal_t(message_out_internal_t &&other) noexcept
            : length(other.length), sent_pos(other.sent_pos),
            m_is_shared(other.m_is_shared),
            m_data_unique(std::move(other.m_data_unique)),
            m_data_shared(std::move(other.m_data_shared))
        {
            other.length = other.sent_pos = 0;
        }

        message_out_internal_t& operator=(message_out_internal_t &&other) noexcept
        {
            length = other.length;
            sent_pos = other.sent_pos;

            m_is_shared = other.m_is_shared;
            m_data_unique = std::move(other.m_data_unique);
            m_data_shared = std::move(other.m_data_shared);

            other.length = other.sent_pos = 0;

            return *this;
        }

        const uint8_t* data()
        {
            if(m_is_shared)
            {
                return m_data_shared.get();
            }
            else
            {
                return m_data_unique.get();
            }
        }

        msg_len_t length;
        msg_len_t sent_pos = 0;

    private:
        // Only one of these smart pointer is used
        // unique_ptr is more efficient but shared_ptr allows to avoid memcpy during multicast
        bool m_is_shared;
        std::unique_ptr<uint8_t[]> m_data_unique;
        std::shared_ptr<uint8_t[]> m_data_shared;
    };
        
    //! Construct as a child socket
    //! Is only called by Socket::accept
    TcpSocket(MessageMode mode, int fd, size_t max_send_queue_size);

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

    MessageSlicer& get_slicer() {
        return *m_slicer;
    } 

    std::atomic<int>& get_fd() {
        return m_fd;
    }

private:
    //! Port used on our side of the connection
    uint16_t m_port;
    bool m_is_ipv6;

    //! File descriptor
    std::atomic<int> m_fd;

    //! The address of the connected client (if any)
    //! Will still be valid after close() was called
    //! Also used to register with the parent socket
    Address m_remote_address;

    std::unique_ptr<MessageSlicer> m_slicer;

    enum class State
    {
        Listening,
        Connected,
        Shutdown,
        Closed,
        Unknown
    };

    std::mutex m_send_queue_mutex;
    std::condition_variable m_send_queue_cond;
    std::vector<message_out_internal_t> m_send_queue;

    std::mutex m_send_mutex;
    std::optional<message_out_internal_t> m_current_message;

    State m_state = State::Unknown;

    // Keep track of the size of outgoing data
    const size_t m_max_send_queue_size;
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

inline bool TcpSocket::send(const uint8_t *data, uint32_t len, bool async)
{
    auto cpy = std::make_unique<uint8_t[]>(len); 
    memcpy(cpy.get(), data, len);

    return send(cpy, len, async);
}

}
