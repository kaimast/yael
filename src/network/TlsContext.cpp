#include "TlsContext.h"

#include <sys/socket.h>
#include <unistd.h>
#include <botan/tls_server.h>
#include <botan/tls_client.h>
#include <glog/logging.h>

#include "MessageSlicer.h"

namespace yael
{
namespace network
{

TlsContext::TlsContext(TlsSocket &socket)
    : m_socket(socket), m_session_mgr(m_rng)
{
}

void TlsContext::send(const Socket::message_out_t &message)
{
    m_channel->send(message.data, message.length);
}

void TlsContext::tls_emit_data(const uint8_t data[], size_t size)
{
    uint32_t sent = 0;

    while(sent < size)
    {
        auto s = ::write(m_socket.m_fd, data, size - sent);

        if(s > 0)
        {
            sent += s;
        }
        else if(s == 0)
        {
            LOG(WARNING) << "Connection lost during send: Message may only be sent partially";
            m_socket.close();
            return;
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
                DLOG(WARNING) << "Received EPIPE";
                m_socket.close();
                return;
            default:
                m_socket.close();
                throw socket_error(strerror(errno));
            }
        }
    }
}

void TlsContext::tls_record_received(uint64_t seq_no, const uint8_t data[], size_t size)
{
    auto &slicer = *m_socket.m_slicer;
    auto &buffer = slicer.buffer();

    if(buffer.is_valid())
    {
        throw std::runtime_error("Can't receive data right now");
    }

    if(buffer.MAX_SIZE < size)
    {
        throw std::runtime_error("Data too long");
    }

    memcpy(buffer.data, data, size);
    buffer.size = size;
    buffer.position = 0;

    slicer.process_buffer();
}

void TlsContext::tls_alert(Botan::TLS::Alert alert)
{

}

bool TlsContext::tls_session_established(const Botan::TLS::Session &session)
{
    return true;
}

TlsServer::TlsServer(TlsSocket &socket)
    : TlsContext(socket)
{
    m_channel = std::make_unique<Botan::TLS::Server>
        (*this, m_session_mgr, m_credentials, m_policy, m_rng);
}

TlsClient::TlsClient(TlsSocket &socket)
    : TlsContext(socket)
{
    m_channel = std::make_unique<Botan::TLS::Client>
        (*this, m_session_mgr, m_credentials, m_policy, m_rng,
        Botan::TLS::Server_Information("botan.randombit.net", 443),
        Botan::TLS::Protocol_Version::TLS_V12);
}


}
}
