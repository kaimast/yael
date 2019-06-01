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

#include "DatagramMessageSlicer.h"
#include "StreamMessageSlicer.h"

using namespace std;

namespace yael::network
{

constexpr int TRUE_FLAG = 1;

TcpSocket::TcpSocket(MessageMode mode, size_t max_send_queue_size)
    : m_port(0), m_is_ipv6(false), m_fd(-1), m_max_send_queue_size(max_send_queue_size)
{
    if(mode == MessageMode::Datagram)
    {
        m_slicer = std::make_unique<DatagramMessageSlicer>();
    }
    else if(mode == MessageMode::Stream)
    {
        m_slicer = std::make_unique<StreamMessageSlicer>();
    }
    else
    {
        throw std::runtime_error("Invalid message mode");
    }
}

TcpSocket::TcpSocket(MessageMode mode, int fd, size_t max_send_queue_size)
    : m_port(0), m_is_ipv6(false), m_fd(fd), m_max_send_queue_size(max_send_queue_size)
{
    if(mode == MessageMode::Datagram)
    {
        m_slicer = std::make_unique<DatagramMessageSlicer>();
    }
    else if(mode == MessageMode::Stream)
    {
        m_slicer = std::make_unique<StreamMessageSlicer>();
    }
    else
    {
        throw std::runtime_error("Invalid message mode");
    }

    uint32_t flags = fcntl(m_fd, F_GETFL, 0);
    flags = flags | O_NONBLOCK;
    fcntl(m_fd, F_SETFL, flags);

    update_port_number();
    calculate_remote_address();

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

    uint32_t flags = fcntl(m_fd, F_GETFL, 0);
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
    uint32_t flags = fcntl(m_fd, F_GETFL, 0);
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

    calculate_remote_address();
    update_port_number();

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

std::vector<std::unique_ptr<Socket>> TcpSocket::accept()
{
    if(!is_listening())
    {
        throw socket_error("Cannot accept on connected TcpTcpSocket");
    }

    std::vector<std::unique_ptr<Socket>> res;

    while(true)
    {
        auto fd = internal_accept();
        
        if(fd >= 0)
        {
            auto ptr = new TcpSocket(m_slicer->type(), fd, m_max_send_queue_size);
            res.emplace_back(std::unique_ptr<Socket>(ptr));
        }
        else
        {
            return res;
        }
    }
}

const Address& TcpSocket::get_remote_address() const
{
    return m_remote_address;
}

void TcpSocket::calculate_remote_address()
{
    char ipstring[16];
    sockaddr_in sin;
    uint32_t len = sizeof(sin);

    if( getpeername(m_fd, reinterpret_cast<sockaddr*>(&sin), &len) == -1)
    {
        m_remote_address.reset();
    }

    inet_ntop( AF_INET, dynamic_cast<in_addr*>(&sin.sin_addr), &ipstring[0], 16);

    uint16_t port = htons(sin.sin_port);

    m_remote_address.IP = &ipstring[0];
    m_remote_address.PortNumber = port;
}

bool TcpSocket::close(bool fast)
{
    if(m_state == State::Connected && !fast)
    {
        m_state = State::Shutdown;
        int i = ::shutdown(m_fd, SHUT_RD | SHUT_WR);
        (void)i; //unused

        return false;
    }

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
            LOG(FATAL) << "Invalid state";
        }
        
        //no-op
    }
    
    return true;
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

    m_slicer->prepare_message(data, len);

    auto msg_out = message_out_internal_t(std::move(data), len);

    {
        std::unique_lock lock(m_send_mutex);

        if(m_send_queue_size >= m_max_send_queue_size)
        {
            throw send_queue_full();
        }

        m_send_queue_size += msg_out.length;
        m_send_queue.emplace_back(std::move(msg_out));
    }

    return do_send();
}

void TcpSocket::wait_send_queue_empty()
{
    std::unique_lock lock(m_send_mutex);

    while(m_send_queue_size > 0)
    {
        m_send_queue_cond.wait(lock);
    }
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
        
        const uint32_t length = message.length;
        const uint8_t *rdata = message.data.get();

        while(message.sent_pos < length)
        {
            auto s = ::write(m_fd, rdata + message.sent_pos, length - message.sent_pos);

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

        m_send_queue_size -= message.length;
        m_send_queue.erase(it);
        m_send_queue_cond.notify_all();
    }
}

} //namespace yael::network
