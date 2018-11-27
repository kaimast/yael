#pragma once

#include <botan/tls_callbacks.h>
#include <botan/tls_channel.h>
#include <botan/tls_session_manager.h>
#include <botan/auto_rng.h>
#include <botan/pk_keys.h>

#include <condition_variable>
#include <mutex>

#include "yael/network/TlsSocket.h"
#include "TlsPolicy.h"
#include "ClientCredentials.h"
#include "ServerCredentials.h"

namespace yael
{
namespace network
{

class TlsContext:  public Botan::TLS::Callbacks
{
public:
    TlsContext(TlsSocket &socket);
    virtual ~TlsContext() = default;

    void send(const uint8_t *data, uint32_t length);

    void wait_connected();

    void tls_process_data(buffer_t &buffer);

    void close();

protected:
    /// Incoming data
    void tls_emit_data(const uint8_t data[], size_t size) override;

    void tls_record_received(uint64_t seq_no, const uint8_t data[], size_t size) override;

    void tls_alert(Botan::TLS::Alert alert) override;

    bool tls_session_established(const Botan::TLS::Session &session) override;

    void tls_verify_cert_chain(const std::vector<Botan::X509_Certificate>& cert_chain,
             const std::vector<std::shared_ptr<const Botan::OCSP::Response>>& ocsp_responses,
             const std::vector<Botan::Certificate_Store*>& trusted_roots,
             Botan::Usage_Type usage,
             const std::string& hostname,
             const Botan::TLS::Policy& policy) override;

    std::mutex m_mutex;
    std::condition_variable m_cond_var;

    TlsSocket &m_socket;
    
    Botan::AutoSeeded_RNG m_rng;
    Botan::TLS::Session_Manager_In_Memory m_session_mgr;
    TlsPolicy m_policy;

    /// This is either a server or client
    std::unique_ptr<Botan::TLS::Channel> m_channel;
};

class TlsServer : public TlsContext
{
public:
    TlsServer(TlsSocket &socket, const std::string &key_path, const std::string &cert_path);

private:
    ServerCredentials m_credentials;
};

class TlsClient : public TlsContext
{
public:
    TlsClient(TlsSocket &socket);

private:
    ClientCredentials m_credentials;
};


}
}
