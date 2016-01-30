#pragma once

#include <list>
#include <vector>
#include <stdint.h>
#include <mutex>
#include <tuple>
#include <stdexcept>

#include "network/Address.h"

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

    struct datagram_out_t
    {
        const char *data;
        const uint32_t length;
    };

    struct datagram_in_t
    {
        char *data;
        uint32_t length;
    };

    typedef std::vector<datagram_out_t> message_out_t;
    typedef std::vector<datagram_in_t> message_in_t;

    static void free_message(message_in_t& message)
    {
        for(auto datagram: message)
            delete []datagram.data;
    }

    Socket(const Socket& other) = delete;
    Socket();
    ~Socket();

    //! Accept new connections
    Socket* accept(bool nonblocking = false);

    //! accepts as much client as possible without blocking
    std::vector<Socket*> accept_all();

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

    //! Receive data
    bool receive(message_in_t& message, bool block = false) throw (std::runtime_error) __attribute__((warn_unused_result));
    bool receive(char*& data, uint32_t& length, bool block = false) throw (std::runtime_error) __attribute__((warn_unused_result));

    //! Send a message consisting of a list of datagrams
    bool send(const message_out_t& message) throw(std::runtime_error) __attribute__((warn_unused_result));

    //! Send either raw data or string
    bool send(const char* data, const uint32_t length) throw(std::runtime_error) __attribute__((warn_unused_result));

    //! Return the first datagram of the first message
    bool peek(datagram_in_t &head) const;

    int32_t getMaximumMessageSize() const;

    uint16_t get_listening_port() const;

    bool is_connected() const;

    bool is_listening() const;

    const Address& get_client_address() const;

    void set_blocking(bool blocking);

    int32_t get_fileno() const;

    //! Reads as much data as possible without blocking
    std::vector<message_in_t> receive_all(bool blocking);

    //! Pull new messages from the socket onto our stack
    bool pull_messages(bool blocking) throw (std::runtime_error);

protected:
    //! Construct as a child socket
    //! Is only called by Socket::accept
    Socket(int fd, uint16_t port);

private:
    struct __attribute__((packed))
    header_t
    {
        header_t(uint32_t length_, bool has_more_)
          : length(length_), has_more(has_more_)
        {}

        header_t(const header_t &other)
          : length(other.length), has_more(other.has_more)
        {}

        uint32_t length;
        bool has_more;
    };

    struct internal_message_in_t
    {
        internal_message_in_t()
            : datagrams(), current_header(0, false), datagram_pos(0), read_pos(0)
        {}

        internal_message_in_t(const internal_message_in_t& other)
            : datagrams(other.datagrams), current_header(other.current_header), datagram_pos(other.datagram_pos), read_pos(other.read_pos)
        {
        }

        std::vector<datagram_in_t> datagrams;

        header_t current_header;

        uint32_t datagram_pos;
        uint32_t read_pos;
    };

    bool create_fd();

    //! Take a message from the stack (if any)
    bool get_message(message_in_t& message);

    bool fetch_more(bool blocking);

    //! Receive data from the socket
    //! Only used by pullMessages
    bool receive_data(bool retry) throw (std::runtime_error);

    //! (Re)calculate mClientAddress
    //! Called by constructor and connect()
    void calculate_client_address();

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

inline int32_t Socket::getMaximumMessageSize() const
{
    return -1;
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
    Address addr = resolveUrl(name, port);
    return listen(addr, backlog);
}

inline bool Socket::receive(char *&data, uint32_t &length, bool block) throw(std::runtime_error)
{
    message_in_t message;
    bool result = receive(message, block);

    if(message.size() > 1)
        throw std::runtime_error("Message has more than one datagram!");

    data = message[0].data;
    length = message[0].length;

    return result;
}

inline bool Socket::send(const char *data, const uint32_t length) throw(std::runtime_error)
{
    datagram_out_t datagram = {data, length};
    return send( {datagram} );
}

}
}
