#include "yael/network/TlsSocket.h"

#include "TlsContext.h"

#include <glog/logging.h>

namespace yael::network
{

TlsSocket::TlsSocket(MessageMode mode, std::string key_path, std::string cert_path, size_t max_send_queue_size)
    : TcpSocket(mode, max_send_queue_size), m_key_path(std::move(key_path)), m_cert_path(std::move(cert_path))
{
    m_state = State::Unknown;
}

TlsSocket::TlsSocket(MessageMode mode, int32_t fd, std::string key_path, std::string cert_path, size_t max_send_queue_size)
    : TcpSocket(mode, fd, max_send_queue_size), m_key_path(std::move(key_path)), m_cert_path(std::move(cert_path))
{
    m_state = State::Setup;
    m_tls_context = std::make_unique<TlsServer>(*this, m_key_path, m_cert_path);
}

TlsSocket::~TlsSocket()
{
    TlsSocket::close(true);
}

std::vector<std::unique_ptr<Socket>> TlsSocket::accept()
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
            auto ptr = new TlsSocket(m_slicer->type(), fd, m_key_path, m_cert_path,max_send_queue_size());
            res.emplace_back(std::unique_ptr<Socket>(ptr));
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

bool TlsSocket::close(bool fast)
{
    if(m_tls_context && m_state == State::Connected && !fast)
    {
        m_state = State::Shutdown;
        m_tls_context->close();
        return false;
    }

    m_state = State::Closed;
    return TcpSocket::close(fast);
}

bool TlsSocket::send(const uint8_t *data, uint32_t length)
{
    if(m_state != State::Connected)
    {
        return false;
    }

    try {
        m_tls_context->send(data, length);
    } catch(std::exception &e) {
        LOG(WARNING) << "Failed to send data: " << e.what();
        close();
        return false;
    }

    return do_send();
}

bool TlsSocket::do_send()
{
    return TcpSocket::do_send();
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

} // namespace yael::network
