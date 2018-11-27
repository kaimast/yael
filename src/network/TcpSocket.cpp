#include "yael/network/TcpSocket.h"

#include <sstream>
#include <fcntl.h>
#include <sys/socket.h>
#include <sys/poll.h>
#include <sys/types.h>
#include <csignal>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <iostream>
#include <cerrno>
#include <stdexcept>
#include <cstring>
#include <unistd.h>
#include <cassert>
#include <glog/logging.h>

#include "MessageSlicer.h"

using namespace std;

namespace yael
{

namespace network
{

constexpr int TRUE_FLAG = 1;

TcpSocket::TcpSocket()
    : m_port(0), m_is_ipv6(false), m_fd(-1)
{
    m_slicer = std::make_unique<MessageSlicer>();
}

TcpSocket::TcpSocket(int fd)
    : m_port(0), m_is_ipv6(false), m_fd(fd)
{
    m_slicer = std::make_unique<MessageSlicer>();

    int flags = fcntl(m_fd, F_GETFL, 0);
    flags = flags | O_NONBLOCK;
    fcntl(m_fd, F_SETFL, flags);

    update_port_number();
    calculate_client_address();

    m_state = State::Connected;
}

TcpSocket::~TcpSocket()
{
    TcpSocket::close(true);
}

bool TcpSocket::has_messages() const
{
    return m_slicer->has_messages();
}

bool TcpSocket::create_fd()
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

void TcpSocket::update_port_number()
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

bool TcpSocket::bind_socket(const Address& address)
{
    m_is_ipv6 = address.IPv6;

    if(!create_fd())
    {
        return false;
    }

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

bool TcpSocket::listen(const Address& address, uint32_t backlog)
{
    if(!bind_socket(address))
    {
        throw socket_error("Failed to bind socket!");
    }

    int flags = fcntl(m_fd, F_GETFL, 0);
    flags = flags | O_NONBLOCK;
    fcntl(m_fd, F_SETFL, flags);

    if(::listen(m_fd, backlog) == 0)
    {
        m_state = State::Listening;
        return true;
    }
    else
    {
        return false;
    }
}

uint16_t TcpSocket::port() const
{
    if(!is_valid())
    {
        throw socket_error("Cannot get port of non-existant socket");
    }

    return m_port;
}

bool TcpSocket::connect(const Address& address, const std::string& name)
{
    if(address.PortNumber == 0)
    {
        throw std::invalid_argument("Need to specify a port number");
    }

    if(name.empty())
    {
        m_is_ipv6 = address.IPv6;
        if(!create_fd())
        {
            throw socket_error(strerror(errno));
        }
    }
    else
    {
        Address my_addr = resolve_URL(name, ANY_PORT, address.IPv6);

        if(!bind_socket(my_addr))
        {
            throw socket_error(strerror(errno));
        }
    }

    // Set it blocking just for connect
    int flags = fcntl(m_fd, F_GETFL, 0);
    flags = flags & ~O_NONBLOCK;
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

    m_state = State::Connected;
    return true;
}

int32_t TcpSocket::internal_accept()
{
    int32_t s = 0;

    if(m_is_ipv6)
    {
        sockaddr_in6 sock_addr;
        socklen_t len = sizeof(sock_addr);
        s = ::accept(m_fd, reinterpret_cast<sockaddr*>(&sock_addr), &len);
    }
    else
    {
        sockaddr_in sock_addr;
        socklen_t len = sizeof(sock_addr);
        s = ::accept(m_fd, reinterpret_cast<sockaddr*>(&sock_addr), &len);
    }

    if(s < 0 && errno != EWOULDBLOCK)
    {
        close();
        std::string str = "Failed to accept new connection; ";
        str += strerror(errno);
        throw socket_error(str);
    }

    return s;
}

std::vector<Socket*> TcpSocket::accept()
{
    if(!is_listening())
    {
        throw socket_error("Cannot accept on connected TcpTcpSocket");
    }

    std::vector<Socket*> res;

    while(true)
    {
        auto fd = internal_accept();
        
        if(fd >= 0)
        {
            res.push_back(new TcpSocket(fd));
        }
        else
        {
            return res;
        }
    }
}

const Address& TcpSocket::get_client_address() const
{
    return m_client_address;
}

void TcpSocket::calculate_client_address()
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

void TcpSocket::close(bool fast)
{
    if(m_state == State::Connected && !fast)
    {
        m_state = State::Shutdown;
        int i = ::shutdown(m_fd, SHUT_RD | SHUT_WR);
        (void)i; //unused
    }
    else
    {
        if(m_fd > 0)
        {
            m_state = State::Closed;
            int i = ::close(m_fd);
            (void)i; //unused
            m_fd = -1;

            m_slicer->buffer().reset();
        }
        else
        {
            if(!(m_state == State::Closed || m_state == State::Unknown))
            {
                throw socket_error("Invalid state");
            }

            //no-op
        }
    }
}

void TcpSocket::pull_messages() 
{
    auto &buffer = m_slicer->buffer();

    if(!buffer.is_valid())
    {
        bool res = receive_data(buffer);
        if(!res)
        {
            return;
        }
    }

    try
    {
        m_slicer->process_buffer();
    }
    catch(std::exception &e)
    {
        // ignore
    }

    // read rest of buffer
    // always pull more until we get EAGAIN
    pull_messages();
}

bool TcpSocket::receive_data(buffer_t &buffer)
{
    if(!is_valid())
    {
        return false;
    }

    if(buffer.is_valid())
    {
        throw std::runtime_error("TcpSocket::receive_data failed: Still have data queued up in buffer");
    }

    memset(&buffer.data[0], 0, yael::network::buffer_t::MAX_SIZE);
    auto x = ::recv(m_fd, buffer.data, yael::network::buffer_t::MAX_SIZE, 0);

    // Now act accordingly
    // > 0 -> data
    // = 0 -> disconnect
    // < 0 -> error/block
    if(x > 0)
    {
        buffer.size = x;
        buffer.position = 0;

        return true;
    }
    else if(x == 0)
    {
        close(true);
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
            close(true);
            break;
        default:
        {
            std::string str = "Failed to receive data; ";
            str += strerror(errno);

            // First close socket and then throw the error!
            close(true);
            throw socket_error(str);
        }
        }

        return false;
    }
}

std::optional<TcpSocket::message_in_t> TcpSocket::receive()
{
    pull_messages();

    if(m_slicer->has_messages())
    {
        message_in_t msg;
        auto res = m_slicer->get_message(msg);
        if(!res)
        {
            throw socket_error("failed to get message");
        }
        
        return { msg };
    }
    else
    {
        return {};
    }
}

bool TcpSocket::send(std::unique_ptr<uint8_t[]> &&data, uint32_t len)
{
    if(len <= 0)
    {
        throw socket_error("Message size has to be > 0");
    }

    auto msg_out = message_out_internal_t(std::move(data), len);

    {
        std::unique_lock lock(m_send_mutex);

        if(m_send_queue.size() > 100)
        {
            throw socket_error("Send queue is full");
        }

        m_send_queue.emplace_back(std::move(msg_out));
    }

    return do_send();
}

bool TcpSocket::do_send()
{
    std::unique_lock lock(m_send_mutex);
    
    while(true)
    {
        auto it = m_send_queue.begin();
        
        if(it == m_send_queue.end())
        {
            // we sent everything!
            return false;
        }

        if(!is_valid())
        {
            throw socket_error("Cannot send data on invalid port");
        }

        auto &message = *it;
        
        const uint32_t length = message.length + MessageSlicer::HEADER_SIZE;
        const uint8_t *rdata = message.data.get();

        while(message.sent_pos < length)
        {
            int32_t s = 0;

            if(message.sent_pos < MessageSlicer::HEADER_SIZE)
            {
                s = ::write(m_fd, reinterpret_cast<const char*>(&length)+ message.sent_pos, MessageSlicer::HEADER_SIZE - message.sent_pos);
            }
            else
            {
                s = ::write(m_fd, rdata+ (message.sent_pos - MessageSlicer::HEADER_SIZE), length - message.sent_pos);
            }

            if(s > 0)
            {
                message.sent_pos += s;
            }
            else if(s == 0)
            {
                LOG(WARNING) << "Connection lost during send: Message may only be sent partially";
                close(true);
                return false;
            }
            else if(s < 0)
            {
                auto e = errno;

                switch(e)
                {
                case EAGAIN:
                case ECONNRESET:
                    // we did not finish sending
                    return true;
                    break;
                case EPIPE:
                    close(true);
                    return false;
                default:
                    close(true);
                    throw socket_error(strerror(errno));
                }
            }
        }

        m_send_queue.erase(it);
    }
}

}
}
