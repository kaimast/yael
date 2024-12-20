#pragma once

#include <vector>
#include <string>
#include <stdexcept>
#include <cstdint>
#include <optional>
#include <iostream>

#include "Address.h"
#include "MessageSlicer.h"

namespace yael::network {

enum class ProtocolType : uint8_t
{
    TCP,
    TLS
};

inline std::ostream& operator<<(std::ostream &stream, const ProtocolType &type)
{
    if(type == ProtocolType::TCP)
    {
        stream << "TCP";
    }
    else if(type == ProtocolType::TLS)
    {
        stream << "TLS";
    }
    else
    {
        throw std::runtime_error("Invalid socket type");
    }

    return stream;
}

class socket_error : public std::exception
{
public:
    explicit socket_error(const std::string msg)
        : m_msg(msg)
    {}

    [[nodiscard]]
    const char* what() const noexcept override
    {
        return m_msg.c_str();
    }

private:
    const std::string m_msg;
};

class send_queue_full : public std::exception {};

/// Abstract socket interface
class Socket
{
public:
    static constexpr uint16_t ANY_PORT = 0;

    static void free_message(message_in_t& message)
    {
        delete []message.data;
    }

    virtual ~Socket() = default;

    //! Accept new connections
    virtual std::vector<std::unique_ptr<Socket>> accept() = 0;

    [[nodiscard]]
    virtual bool has_messages() const = 0;

    //! Connect to an address
    virtual bool connect(const Address& address, const std::string& name = "") __attribute__((warn_unused_result)) = 0;

    //! Wait for the connection to be established
    //! This is a no-op for plain TCP/UDP but will block for encrypted channels
    //! You need to call this after connect() and registering the socket with and event loop
    //! receive() calls will handle establishing of a connnection
    virtual bool wait_connection_established() = 0;

    //! Make the port listen for connections
    virtual bool listen(const Address& address, uint32_t backlog) __attribute__((warn_unused_result)) = 0;

    //! Make the port listen for connections
    //! This will resolve the name to an ip for you
    bool listen(const std::string& name, uint16_t port, uint32_t backlog) __attribute__((warn_unused_result));

    /**
     * @brief close this socket
     * @note this function will not report error on invalid sockets
     */
    virtual bool close(bool fast = false) = 0;

    /**
     * Returns true if there is more data to send
     * In this case you need to invoke do_send() when the socket is writable
     */
    virtual bool send(const uint8_t *data, uint32_t len, bool async = false) __attribute__((warn_unused_result)) = 0;

    virtual bool do_send() __attribute__((warn_unused_result)) = 0;

    // Wait for the send queue to empty
    // @note this will block!
    virtual void wait_send_queue_empty() = 0;

    //! Send either raw data or string
    //! This version will take ownership of data (unless the send queue is full)
    virtual bool send(std::unique_ptr<uint8_t[]> &data, const uint32_t length, bool async = false) __attribute__((warn_unused_result)) = 0;

    /// Same as send but takes a shared pointer as argument
    /// This allows for less memcpy when sending to many parties
    //! This version will take ownership of data (unless the send queue is full)
    virtual bool send(std::shared_ptr<uint8_t[]> &data, uint32_t len, bool async = false) __attribute__((warn_unused_result)) = 0;


    /**
     * Either the listening port or the connection port
     * (depending on the socket state)
     */
    [[nodiscard]]
    virtual uint16_t port() const = 0;

    /**
     * Has a connection and session been established with the remote party
     *
     * Note: For TLS this means that the handshake is completed
     */
    [[nodiscard]]
 
    virtual bool is_connected() const = 0;

    /// Is this socket listening for new connections?
    [[nodiscard]]
 
    virtual bool is_listening() const = 0;

    /// Is this a  valid socket
    /// Note; this does not imply a connection is established yet
    [[nodiscard]]
    virtual bool is_valid() const = 0;

    /// Get the address of the party we are connected to
    [[nodiscard]]
    virtual const Address& get_remote_address() const = 0;

    [[nodiscard]]
    virtual int32_t get_fileno() const = 0;

    /// What is the maximum amount of data that can be queued up? 
    [[nodiscard]]
    virtual size_t max_send_queue_size() const = 0;

    /// How much data is queued to be sent?
    [[nodiscard]]
    virtual size_t send_queue_size() const = 0;

    virtual std::optional<message_in_t> receive() = 0;

    [[nodiscard]]
    virtual const MessageSlicer& message_slicer() const = 0;
};

inline bool Socket::listen(const std::string& name, uint16_t port, uint32_t backlog)
{
    const auto addr = resolve_URL(name, port);
    return listen(addr, backlog);
}

}
