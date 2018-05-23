#pragma once

#include <botan/tls_callbacks.h>
#include <botan/tls_channel.h>
#include <botan/tls_session_manager.h>
#include <botan/tls_policy.h>
#include <botan/auto_rng.h>
#include <botan/pk_keys.h>

#include "yael/network/TlsSocket.h"
#include "ServerCredentials.h"

namespace yael
{
namespace network
{

class TlsContext:  public Botan::TLS::Callbacks
{
public:
    TlsContext(TlsSocket &socket);

    void send(const Socket::message_out_t &message);

    /// Incoming data
    void tls_emit_data(const uint8_t data[], size_t size) override;

    void tls_record_received(uint64_t seq_no, const uint8_t data[], size_t size) override;

    void tls_alert(Botan::TLS::Alert alert) override;

    bool tls_session_established(const Botan::TLS::Session &session) override;

protected:
    TlsSocket &m_socket;

    Botan::AutoSeeded_RNG m_rng;
    Botan::TLS::Session_Manager_In_Memory m_session_mgr;
    Botan::TLS::Strict_Policy m_policy;

    /// This is either a server or client
    std::unique_ptr<Botan::TLS::Channel> m_channel;
};

class TlsServer : public TlsContext
{
public:
    TlsServer(TlsSocket &socket);

private:
    ServerCredentials m_credentials;
};

class TlsClient : public TlsContext
{
public:
    TlsClient(TlsSocket &socket);

private:
    ServerCredentials m_credentials;
};


}
}
