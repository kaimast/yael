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

void TlsContext::send(const uint8_t *data, uint32_t length)
{
    std::unique_lock lock(m_mutex);

    uint32_t header = length + sizeof(uint32_t);
    m_channel->send(reinterpret_cast<const uint8_t*>(&header), sizeof(header));
    m_channel->send(data, length);
}

void TlsContext::wait_connected()
{
    std::unique_lock lock(m_mutex);

    while(!m_socket.is_connected())
    {
        m_cond_var.wait(lock);
    }
}

void TlsContext::tls_process_data(buffer_t &buffer)
{
    if(m_channel)
    {
        // may happe during shutdown
        m_channel->received_data(buffer.data, buffer.size);
    }
}

void TlsContext::tls_verify_cert_chain(const std::vector<Botan::X509_Certificate>& cert_chain,
         const std::vector<std::shared_ptr<const Botan::OCSP::Response>>& ocsp_responses,
         const std::vector<Botan::Certificate_Store*>& trusted_roots,
         Botan::Usage_Type usage,
         const std::string& hostname,
         const Botan::TLS::Policy& policy)
{
    (void)cert_chain;
    (void)ocsp_responses;
    (void)trusted_roots;
    (void)usage;
    (void)hostname;
    (void)policy;
    //FIXME actually verify in release mode
}

void TlsContext::tls_emit_data(const uint8_t data[], size_t size)
{
    uint32_t sent = 0;

    while(sent < size)
    {
        auto s = ::write(m_socket.m_fd, reinterpret_cast<const char*>(data)+sent, size - sent);

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

void TlsContext::close()
{
    m_channel->close();
}

void TlsContext::tls_record_received(uint64_t seq_no, const uint8_t data[], size_t size)
{
    (void)seq_no;

    std::unique_lock lock(m_mutex);

    auto &slicer = *m_socket.m_slicer;
    auto &buffer = slicer.buffer();

    size_t pos = 0;

    while(pos < size)
    {
        if(buffer.is_valid())
        {
            throw std::runtime_error("Invalid state");
        }

        auto cpy_size = std::min<size_t>(yael::network::buffer_t::MAX_SIZE, size-pos);

        memcpy(buffer.data, data+pos, cpy_size);
        buffer.size = cpy_size;
        buffer.position = 0;

        slicer.process_buffer();

        pos += cpy_size;
    }
}

void TlsContext::tls_alert(Botan::TLS::Alert alert)
{
    if(alert.type() == Botan::TLS::Alert::CLOSE_NOTIFY)
    {
        m_socket.close(true);
    }
    else
    {
        LOG(WARNING) << "Received TLS alert " << alert.type_string();
    }
}

bool TlsContext::tls_session_established(const Botan::TLS::Session &session)
{
    (void)session;

    std::unique_lock lock(m_mutex);

    m_socket.m_state = TlsSocket::State::Connected;
    m_cond_var.notify_all();
    
    return false;
}

TlsServer::TlsServer(TlsSocket &socket, const std::string &key_path, const std::string &cert_path)
    : TlsContext(socket), m_credentials(key_path, cert_path)
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
