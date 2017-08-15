#include "yael/network/Socket.h"

#include <sstream>
#include <fcntl.h>
#include <sys/socket.h>
#include <sys/poll.h>
#include <sys/types.h>
#include <signal.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <iostream>
#include <errno.h>
#include <stdexcept>
#include <cstring>
#include <unistd.h>
#include <assert.h>
#include <glog/logging.h>

using namespace std;

constexpr int TRUE_FLAG = 1;
constexpr uint32_t HEADER_SIZE = sizeof(uint32_t);

namespace yael
{

namespace network
{

Socket::Socket()
    : m_messages(), m_port(0), m_is_ipv6(false), m_fd(-1),
      m_buffer_pos(-1), m_buffer_size(0), m_listening(false), m_client_address(),
      m_has_current_message(false), m_current_message()
{
}

Socket::Socket(int fd)
    : m_messages(), m_port(0), m_is_ipv6(false), m_fd(fd),
      m_buffer_pos(-1), m_buffer_size(0), m_listening(false), m_client_address(),
      m_has_current_message(false), m_current_message()
{
    int flags = fcntl(m_fd, F_GETFL, 0);
    flags = flags | O_NONBLOCK;
    fcntl(m_fd, F_SETFL, flags);

    update_port_number();
    calculate_client_address();
}

Socket::~Socket()
{
    close();
}

bool Socket::create_fd()
{
    if(m_is_ipv6)
    {
        m_fd = socket(AF_INET6, SOCK_STREAM, IPPROTO_TCP);
    }
    else
    {
        m_fd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    }

    if(!is_valid())
    {
        return false;
    }

    ::setsockopt(m_fd, IPPROTO_TCP, TCP_NODELAY, reinterpret_cast<const char*>(&TRUE_FLAG), sizeof(TRUE_FLAG));

    return true;
}

void Socket::update_port_number()
{
    if(m_is_ipv6)
    {
        struct sockaddr_in6 addr;
        socklen_t addrlen = sizeof(addr);
        getsockname(m_fd, reinterpret_cast<sockaddr*>(&addr), &addrlen);
        m_port = htons(addr.sin6_port);
    }
    else
    {
        struct sockaddr_in addr;
        socklen_t addrlen = sizeof(addr);
        getsockname(m_fd, reinterpret_cast<sockaddr*>(&addr), &addrlen);
        m_port = htons(addr.sin_port);
    }
}

bool Socket::bind_socket(const Address& address)
{
    m_is_ipv6 = address.IPv6;

    if(!create_fd())
        return false;

    // Reuse address so we can quickly recover from crashes
    ::setsockopt(m_fd, SOL_SOCKET, SO_REUSEADDR, &TRUE_FLAG, sizeof(TRUE_FLAG));

    if(m_is_ipv6)
    {
        sockaddr_in6 sock_addr;
        address.get_sock_address6(sock_addr);

        if(::bind(m_fd, reinterpret_cast<sockaddr*>(&sock_addr), sizeof(sock_addr)) != 0)
        {
            return false;
        }
    }
    else
    {
        sockaddr_in sock_addr;
        address.get_sock_address(sock_addr);

        if(::bind(m_fd, reinterpret_cast<sockaddr*>(&sock_addr), sizeof(sock_addr)) != 0)
        {
            return false;
        }
   }

    update_port_number();
    return true;
}

bool Socket::listen(const Address& address, uint32_t backlog)
{
    if(!bind_socket(address))
    {
        throw std::runtime_error("Failed to bind socket!");
    }

    int flags = fcntl(m_fd, F_GETFL, 0);
    flags = flags | O_NONBLOCK;
    fcntl(m_fd, F_SETFL, flags);

    if(::listen(m_fd, backlog) == 0)
    {
        m_listening = true;
        return true;
    }
    else
    {
        return false;
    }
}

uint16_t Socket::port() const
{
    if(!is_valid())
    {
        throw std::runtime_error("Cannot get port of non-existant socket");
    }

    return m_port;
}

bool Socket::connect(const Address& address, const std::string& name)
{
    if(address.PortNumber == 0)
    {
        throw std::invalid_argument("Need to specify a port number");
    }

    if(name == "")
    {
        m_is_ipv6 = address.IPv6;
        if(!create_fd())
        {
            throw std::runtime_error(strerror(errno));
        }
    }
    else
    {
        Address my_addr = resolve_URL(name, ANY_PORT, address.IPv6);

        if(!bind_socket(my_addr))
        {
            throw std::runtime_error(strerror(errno));
        }
    }

    // Set it blocking just for connect
    int flags = fcntl(m_fd, F_GETFL, 0);
    flags = flags | ~O_NONBLOCK;
    fcntl(m_fd, F_SETFL, flags);

    if(m_is_ipv6)
    {
        sockaddr_in6 sock_addr;
        address.get_sock_address6(sock_addr);

        if(::connect(m_fd, reinterpret_cast<const sockaddr*>(&sock_addr), sizeof(sock_addr)) != 0)
        {
            close();
            return false;
        }
    }
    else
    {
        sockaddr_in sock_addr;
        address.get_sock_address(sock_addr);

        if(::connect(m_fd, reinterpret_cast<const sockaddr*>(&sock_addr), sizeof(sock_addr)) != 0)
        {
            close();
            return false;
        }
    }

    flags = fcntl(m_fd, F_GETFL, 0);
    flags = flags | O_NONBLOCK;
    fcntl(m_fd, F_SETFL, flags);

    calculate_client_address();
    return true;
}

std::vector<Socket*> Socket::accept()
{
    if(!is_listening())
    {
        throw std::runtime_error("Cannot accept on connected TcpSocket");
    }

    std::vector<Socket*> res;

    while(true)
    {
        Address address;
        int32_t s = 0;

        if(m_is_ipv6)
        {
            sockaddr_in6 sock_addr;
            socklen_t len = sizeof(sock_addr);
            s = ::accept(m_fd, reinterpret_cast<sockaddr*>(&sock_addr), &len);
            address = Address(sock_addr);
        }
        else
        {
            sockaddr_in sock_addr;
            socklen_t len = sizeof(sock_addr);
            s = ::accept(m_fd, reinterpret_cast<sockaddr*>(&sock_addr), &len);
            address = Address(sock_addr);
        }

        if(s < 0)
        {
            if(errno != EWOULDBLOCK)
            {
                close();
                std::string str = "Failed to accept new connection; ";
                str += strerror(errno);
                throw std::runtime_error(str);
            }
            else
            {
                return res;
            }
        }
        else
        {
            res.push_back(new Socket(s));
        }
    }
}

const Address& Socket::get_client_address() const
{
    return m_client_address;
}

void Socket::calculate_client_address()
{
    char ipstring[16];
    sockaddr_in sin;
    uint32_t len = sizeof(sin);

    if( getpeername(m_fd, reinterpret_cast<sockaddr*>(&sin), &len) == -1)
    {
        m_client_address.reset();
    }

    inet_ntop( AF_INET, dynamic_cast<in_addr*>(&sin.sin_addr), &ipstring[0], 16);

    uint16_t port = sin.sin_port;

    m_client_address.IP = &ipstring[0];
    m_client_address.PortNumber = port;
}

void Socket::close()
{
    int i = ::close(m_fd);
    (void)i; //unused
    m_fd = -1;
    m_buffer_size = 0;
    m_buffer_pos = -1;
}

bool Socket::get_message(message_in_t& message)
{
    if(!has_messages())
    {
        return false;
    }

    auto& it = m_messages.front();
    message.data = it.data;
    message.length = it.length - HEADER_SIZE;

    m_messages.pop_front();
    return true;
}

void Socket::pull_messages() 
{
    bool received_full_msg = false;

    if(m_buffer_pos < 0)
    {
        bool res = receive_data();
        if(!res)
            return;
    }

    internal_message_in_t msg;

    if(m_has_current_message)
    {
        msg = std::move(m_current_message);
        m_has_current_message = false;
    }
    
    // We need to read the header of the next datagram
    if(msg.read_pos < HEADER_SIZE)
    {
        int32_t readlength = std::min<int32_t>(HEADER_SIZE - msg.read_pos, m_buffer_size - m_buffer_pos);

        assert(readlength > 0);
        mempcpy(reinterpret_cast<char*>(&msg.length)+msg.read_pos, &m_buffer[m_buffer_pos], readlength);

        msg.read_pos += readlength;
        m_buffer_pos += readlength;
        if(msg.read_pos == HEADER_SIZE)
        {
            if(msg.length <= HEADER_SIZE)
            {
                LOG(ERROR) << "Not a valid message";
                close();
                return;
            }

            assert(msg.length > HEADER_SIZE); //FIXME
            msg.data = new uint8_t[msg.length - HEADER_SIZE];
        }
    }

    // Has header?
    if(msg.read_pos >= HEADER_SIZE)
    {
        const int32_t readlength = min(msg.length - msg.read_pos, m_buffer_size - m_buffer_pos);

        if(readlength > 0)
        {
            assert(msg.read_pos >= HEADER_SIZE);
            mempcpy(&msg.data[msg.read_pos - HEADER_SIZE], &m_buffer[m_buffer_pos], readlength);

            msg.read_pos += readlength;
            m_buffer_pos += readlength;
        }

        assert(msg.read_pos <= msg.length);

        if(msg.read_pos == msg.length)
        {
            m_messages.push_back(msg);
            received_full_msg = true;

            m_has_current_message = false;
        }
    }

    if(!received_full_msg)
    {
        m_current_message = std::move(msg);
        m_has_current_message = true;
    }

    // End of buffer.
    if(m_buffer_pos == static_cast<int32_t>(m_buffer_size))
    {
        m_buffer_size = 0;
        m_buffer_pos = -1;
    }
        
    // read rest of buffer
    // always pull more until we get EAGAIN
    pull_messages();
}

bool Socket::receive_data() 
{
    if(!is_valid())
        return false;

    assert(m_buffer_pos < 0 && m_buffer_size == 0);

    memset(&m_buffer[0], 0, BUFFER_SIZE);
    int32_t x = ::recv(m_fd, m_buffer, BUFFER_SIZE, 0);

    // Now act accordingly
    // > 0 -> data
    // = 0 -> disconnect
    // < 0 -> error/block
    if(x > 0)
    {
        m_buffer_size = x;
        m_buffer_pos = 0;

        return true;
    }
    else if(x == 0)
    {
        close();
        return false;
    }
    else
    {
        const int e = errno;

        switch(e)
        {
        case EAGAIN:
            break;
        case ECONNRESET:
            close();
            break;
        default:
        {
            std::string str = "Failed to receive data; ";
            str += strerror(errno);

            // First close socket and then throw the error!
            close();
            throw std::runtime_error(str);
        }
        }

        return false;
    }
}

std::optional<Socket::message_in_t> Socket::receive()
{
    pull_messages();

    if(has_messages())
    {
        message_in_t msg;
        auto res = get_message(msg);
        if(!res)
            throw std::runtime_error("failed to get message");
        
        return { msg };
    }
    else
        return {};
}

bool Socket::send(const message_out_t& message)
{
    if(!is_valid())
    {
        throw std::runtime_error("Cannot send data on invalid port");
    }

    if(message.length <= 0)
    {
        throw std::runtime_error("Message size has to be > 0");
    }

    uint32_t sent = 0;
    const uint32_t length = message.length + HEADER_SIZE;

    while(sent < length)
    {
        int32_t s = 0;

        if(sent < HEADER_SIZE)
            s = ::write(m_fd, reinterpret_cast<const char*>(&length)+sent, HEADER_SIZE-sent);
        else
        {
            assert(sent >= HEADER_SIZE);
            s = ::write(m_fd, message.data+(sent-HEADER_SIZE), length-sent);
        }

        if(s > 0)
            sent += s;
        else if(s == 0)
        {
            close();
            return false;
        }
        else if(s < 0)
        {
            auto e = errno;

            switch(e)
            {
            case EAGAIN:
            case ECONNRESET:
                break;
            case EPIPE:
                close();
                return false;
            default:
                close();
                throw std::runtime_error(strerror(errno));
            }
        }
    }

    return true;
}

}
}
