#pragma once

#include <memory>
#include "Socket.h"
#include "TcpSocket.h"

namespace yael::network
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
    explicit TlsSocket(MessageMode mode, std::string key_path = "", std::string cert_path = "", size_t max_send_queue_size = TcpSocket::DEFAULT_MAX_SEND_QUEUE_SIZE);

    ~TlsSocket() override;

    std::vector<std::unique_ptr<Socket>> accept() override;

    bool connect(const Address& address, const std::string& name = "") override __attribute__((warn_unused_result));

    bool listen(const Address& address, uint32_t backlog) override __attribute__((warn_unused_result));
    using Socket::listen;

    /// Same as TcpSocket::close
    bool close(bool fast = false) override;
    
    bool wait_connection_established() override;

    bool send(const uint8_t *data, uint32_t len, bool async = false) override __attribute__((warn_unused_result));
    bool send(std::unique_ptr<uint8_t[]> &data, uint32_t len, bool async = false) override __attribute__((warn_unused_result));
    bool send(std::shared_ptr<uint8_t[]> &data, uint32_t len, bool async = false) override __attribute__((warn_unused_result));

    bool do_send() override  __attribute__((warn_unused_result));

    [[nodiscard]]
    bool is_connected() const override;

protected:
    //! Construct as a child socket
    //! Is only called by Socket::accept
    TlsSocket(MessageMode mode, int32_t fd, std::string key_path, std::string cert_path, size_t max_send_queue_size);

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

inline bool TlsSocket::send(std::unique_ptr<uint8_t[]> &data, uint32_t len, bool async)
{
    return send(data.get(), len, async);
}

inline bool TlsSocket::send(std::shared_ptr<uint8_t[]> &data, uint32_t len, bool async)
{
    return send(data.get(), len, async);
}

} // namespace yael::network
