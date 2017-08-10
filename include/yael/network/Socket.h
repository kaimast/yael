#pragma once

#include <list>
#include <vector>
#include <stdint.h>
#include <mutex>
#include <tuple>
#include <stdexcept>

#include <yael/network/Address.h>

namespace yael
{
namespace network
{

/**
 * @brief Object-oriented wrapper for a TCP socket
 * @note This implementatio is *not* thread safe. Use the EventLoop for multiple threads polling messages.
 */
class Socket
{
public:
    static constexpr uint16_t ANY_PORT = 0;

    struct message_out_t
    {
        const uint8_t *data = nullptr;
        const uint32_t length = 0;
    };

    struct message_in_t
    {
        uint8_t *data;
        uint32_t length;
    };

    static void free_message(message_in_t& message)
    {
        delete []message.data;
    }

    Socket(const Socket& other) = delete;
    Socket();
    ~Socket();

    //! Accept new connections
    std::vector<Socket*> accept();

    bool has_messages() const;

    //! Connect to an address
    bool connect(const Address& address, const std::string& name = "") __attribute__((warn_unused_result));

    //! Make the port listen for connections
    bool listen(const Address& address, uint32_t backlog) __attribute__((warn_unused_result));

    //! Make the port listen for connections
    //! This will resolve the name to an ip for you
    bool listen(const std::string& name, uint16_t port, uint32_t backlog) __attribute__((warn_unused_result));

    /**
     * @brief close this socket
     * @note this function will not report error on invalid sockets
     */
    void close();

    /**
     * @brief Is this a valid socket? (i.e. either listening or connected)
     */
    bool is_valid() const;

    //! Send a message consisting of a list of datagrams
    bool send(const message_out_t& message) __attribute__((warn_unused_result));

    //! Send either raw data or string
    bool send(const uint8_t* data, const uint32_t length) __attribute__((warn_unused_result));

    /**
     * Either the listening port or the connection port
     * (depending on the socket state)
     */
    uint16_t port() const;

    bool is_connected() const;

    bool is_listening() const;

    const Address& get_client_address() const;

    int32_t get_fileno() const;

    //! Reads as much data as possible without blocking
    std::vector<message_in_t> receive_all();

protected:
    //! Construct as a child socket
    //! Is only called by Socket::accept
    Socket(int fd);

private:
    //! Pull new messages from the socket onto our stack
    void pull_messages();

    struct internal_message_in_t
    {
        internal_message_in_t()
            : length(0), read_pos(0), data(nullptr)
        {
        }

        internal_message_in_t(const internal_message_in_t &other)
            : length(other.length), read_pos(other.read_pos), data(other.data)
        {
        }

        void operator=(internal_message_in_t &&other)
        {
            if(!other.valid())
                throw std::runtime_error("other's not valid");

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

        uint32_t length;
        uint32_t read_pos;
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
    char m_buffer[BUFFER_SIZE];

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

inline int32_t Socket::get_fileno() const
{
    return m_fd;
}

inline bool Socket::is_connected() const
{
    return is_valid() && !m_listening;
}

inline bool Socket::is_listening() const
{
    return is_valid() && m_listening;
}

inline bool Socket::has_messages() const
{
    return m_messages.size() > 0;
}

inline bool Socket::is_valid() const
{
    return m_fd >= 0;
}

inline bool Socket::listen(const std::string& name, uint16_t port, uint32_t backlog)
{
    Address addr = resolve_URL(name, port);
    return listen(addr, backlog);
}

inline bool Socket::send(const uint8_t *data, const uint32_t length) 
{
    message_out_t msg = {data, length};
    return send(msg);
}

}
}
