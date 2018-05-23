#include "yael/network/TlsSocket.h"

#include "TlsContext.h"

namespace yael
{
namespace network
{

TlsSocket::TlsSocket()
{
}

TlsSocket::TlsSocket(int32_t fd)
    : TcpSocket(fd)
{
    m_tls_context = std::make_unique<TlsServer>(*this);
}

std::vector<Socket*> TlsSocket::accept()
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
            res.push_back(new TlsSocket(fd));
        }
        else
        {
            return res;
        }
    }
    return {}; //fixme
}

bool TlsSocket::connect(const Address& address, const std::string& name)
{
    m_tls_context = std::make_unique<TlsClient>(*this);
    return false; //fixme
}

bool TlsSocket::listen(const Address& address, uint32_t backlog)
{
    return TcpSocket::listen(address, backlog);
}

void TlsSocket::close()
{
    TcpSocket::close();
}

bool TlsSocket::send(const message_out_t& message)
{
    m_tls_context->send(message);
    return true; //fixme
}

bool TlsSocket::is_connected() const
{
    return false; //fixme
}

void TlsSocket::queue_message(const uint8_t *data, size_t size)
{
    Socket::message_in_t msg;

    msg.data = new uint8_t[size];
    msg.length = size;
    memcpy(msg.data, data, size);

    m_messages.emplace_back(std::move(msg));
}

void TlsSocket::pull_messages()
{

}

}
}
