#pragma once

#include <memory>
#include "Socket.h"
#include "TcpSocket.h"

namespace yael
{
namespace network
{

class TlsContext;

class TlsSocket : public TcpSocket
{
public:
    /**
     * Constructor
     *
     * You only need to specify key and certificate for the server
     */
    TlsSocket(const std::string &key_path = "", const std::string &cert_path = "");

    ~TlsSocket();

    std::vector<Socket*> accept() override;

    bool connect(const Address& address, const std::string& name = "") override __attribute__((warn_unused_result));

    bool listen(const Address& address, uint32_t backlog) override __attribute__((warn_unused_result));
    using Socket::listen;

    void close() override;
    
    bool wait_connection_established() override;

    bool send(const message_out_t& message) override __attribute__((warn_unused_result));
    using Socket::send;

    bool is_connected() const override;

protected:
    //! Construct as a child socket
    //! Is only called by Socket::accept
    TlsSocket(int32_t fd, const std::string &key_path, const std::string &cert_path);

private:
    void pull_messages() override;

    friend class TlsContext;

    const std::string m_key_path;
    const std::string m_cert_path;

    enum class State
    {
        Unknown,
        Listening,
        Setup,
        Connected,
        Shutdown,
        Closed
    };

    std::unique_ptr<TlsContext> m_tls_context;

    // Buffer for raw data 
    buffer_t m_buffer;

    State m_state;
};

}
}
