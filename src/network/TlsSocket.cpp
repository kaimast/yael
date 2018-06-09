#include "yael/network/TlsSocket.h"

#include "MessageSlicer.h"
#include "TlsContext.h"

#include <glog/logging.h>

namespace yael
{
namespace network
{

TlsSocket::TlsSocket(const std::string &key_path, const std::string &cert_path)
    : m_key_path(key_path), m_cert_path(cert_path)
{
    m_state = State::Unknown;
}

TlsSocket::TlsSocket(int32_t fd, const std::string &key_path, const std::string &cert_path)
    : TcpSocket(fd), m_key_path(key_path), m_cert_path(cert_path)
{
    m_state = State::Setup;
    m_tls_context = std::make_unique<TlsServer>(*this, key_path, cert_path);
}

TlsSocket::~TlsSocket()
{
    TlsSocket::close(true);
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
            res.push_back(new TlsSocket(fd, m_key_path, m_cert_path));
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
    if(!TcpSocket::connect(address, name))
    {
        return false;
    }

    m_tls_context = std::make_unique<TlsClient>(*this);
    return true;
}

bool TlsSocket::wait_connection_established()
{
    m_tls_context->wait_connected();
    return is_connected();
}

bool TlsSocket::listen(const Address& address, uint32_t backlog)
{
    m_state = State::Listening;
    return TcpSocket::listen(address, backlog);
}

void TlsSocket::close(bool fast)
{
    if(m_tls_context && m_state == State::Connected
        && !fast)
    {
        m_state = State::Shutdown;
        m_tls_context->close();
    }

    m_state = State::Closed;
    TcpSocket::close(fast);
}

bool TlsSocket::send(const message_out_t& message)
{
    if(m_state != State::Connected)
    {
        return false;
    }

    try {
        m_tls_context->send(message);
        return true;
    } catch(std::exception &e) {
        LOG(WARNING) << "Failed to send data: " << e.what();
        close();
        return false;
    }   
}

bool TlsSocket::is_connected() const
{
    return m_state == State::Connected;
}

void TlsSocket::pull_messages() 
{
    bool res = receive_data(m_buffer);

    if(!res)
    {
        return;
    }

    m_tls_context->tls_process_data(m_buffer);
    m_buffer.reset();

    // always pull more until we get EAGAIN
    pull_messages();
}

}
}
