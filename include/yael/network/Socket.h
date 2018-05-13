#pragma once

#include <string>
#include <stdexcept>
#include <stdint.h>
#include <functional>
#include <optional>

#include "Address.h"

namespace yael
{
namespace network
{

class socket_error : public std::exception
{
public:
    socket_error(const std::string &msg)
        : m_msg(msg)
    {}

    const char* what() const noexcept override
    {
        return m_msg.c_str();
    }

private:
    const std::string m_msg;
};

using msg_len_t = uint32_t;

/// Abstract socket interface
class Socket
{
public:
    static constexpr uint16_t ANY_PORT = 0;

    struct message_out_t
    {
        const uint8_t *data = nullptr;
        const msg_len_t length = 0;
    };

    struct message_in_t
    {
        uint8_t *data;
        msg_len_t length;
    };

    static void free_message(message_in_t& message)
    {
        delete []message.data;
    }

    virtual ~Socket() = default;

    virtual void set_close_hook(std::function<void()> func) = 0;

    //! Accept new connections
    virtual std::vector<Socket*> accept() = 0;

    virtual bool has_messages() const = 0;

    //! Connect to an address
    virtual bool connect(const Address& address, const std::string& name = "") __attribute__((warn_unused_result)) = 0;

    //! Make the port listen for connections
    virtual bool listen(const Address& address, uint32_t backlog) __attribute__((warn_unused_result)) = 0;

    //! Make the port listen for connections
    //! This will resolve the name to an ip for you
    bool listen(const std::string& name, uint16_t port, uint32_t backlog) __attribute__((warn_unused_result));

    /**
     * @brief close this socket
     * @note this function will not report error on invalid sockets
     */
    virtual void close() = 0;

    /**
     * @brief Is this a valid socket? (i.e. either listening or connected)
     */
    virtual bool is_valid() const = 0;

    //! Send a message consisting of a list of datagrams
    virtual bool send(const message_out_t& message) __attribute__((warn_unused_result)) = 0;

    //! Send either raw data or string
    bool send(const uint8_t* data, const uint32_t length) __attribute__((warn_unused_result));

    /**
     * Either the listening port or the connection port
     * (depending on the socket state)
     */
    virtual uint16_t port() const = 0;

    virtual bool is_connected() const = 0;

    virtual bool is_listening() const = 0;

    virtual const Address& get_client_address() const = 0;

    virtual int32_t get_fileno() const = 0;

    virtual std::optional<message_in_t> receive() = 0;
};

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
