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
    TlsSocket();

    std::vector<Socket*> accept() override;

    bool connect(const Address& address, const std::string& name = "") override __attribute__((warn_unused_result));

    bool listen(const Address& address, uint32_t backlog) override __attribute__((warn_unused_result));
    using Socket::listen;

    void close() override;

    bool send(const message_out_t& message) override __attribute__((warn_unused_result));
    using Socket::send;

    bool is_connected() const override;

protected:
    //! Construct as a child socket
    //! Is only called by Socket::accept
    TlsSocket(int32_t fd);

    void queue_message(const uint8_t *data, size_t size);

private:
    void pull_messages() override;

    friend class TlsContext;

    std::list<Socket::message_in_t> m_messages;

    std::unique_ptr<TlsContext> m_tls_context;
};

}
}
