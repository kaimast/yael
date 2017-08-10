#pragma once

#include <mutex>
#include <memory>
#include <glog/logging.h>

#include "network/Socket.h"
#include "SocketListener.h"

namespace yael
{

class NetworkSocketListener: public SocketListener
{
public:
    /**
     * @brief Construct with a valid socket
     */
    NetworkSocketListener(std::unique_ptr<network::Socket> &&socket);

    /**
     * @brief Construct with a valid socket -- alternate version
     */
    NetworkSocketListener(network::Socket *socket);

    /**
     * @brief Construct without a valid socket (yet)
     */
    NetworkSocketListener();

    NetworkSocketListener(const SocketListener& other) = delete;
    virtual ~NetworkSocketListener();

    /**
     * @brief Hand a valid socket to the listener
     * @param socket the socket
     * @throw std::runtime_error if there is already a socket assigned to this listener
     */
    void set_socket(std::unique_ptr<network::Socket> &&socket) ;
    void set_socket(network::Socket* socket);

    /**
     * @brief get the current or most recent fileno associated with this listener
     * @return -1 if there was never a valid Socket associated with this listener
     */
    int32_t get_fileno() const override;

    /**
     * @brief update will be called when there is potentially new data for the socket
     * @note you have to handle this depending on what your socket does, e.g you might want to call accept() or receive()
     */
    virtual void update() = 0;

    /**
     * @brief returns true if the associated socket is connected
     */
    bool is_valid() const override;

protected:
    network::Socket& socket()
    {
        return *m_socket;
    }

    const network::Socket& socket() const
    {
        return *m_socket;
    }

private:
    std::unique_ptr<network::Socket> m_socket;
    int32_t m_fileno;
};

inline void NetworkSocketListener::set_socket(network::Socket *socket) 
{
    set_socket(std::unique_ptr<network::Socket>(socket));
}

}

