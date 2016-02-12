#pragma once

#include <mutex>
#include <memory>
#include <chrono>
#include <glog/logging.h>

#include "network/Socket.h"

namespace yael
{

/**
 * @brief The SocketListener class
 */
class SocketListener
{
public:
    /**
     * @brief Construct with a valid socket
     */
    SocketListener(std::unique_ptr<network::Socket> &&socket);

    /**
     * @brief Construct with a valid socket -- alternate version
     */
    SocketListener(network::Socket *socket);

    /**
     * @brief Construct without a valid socket (yet)
     */
    SocketListener();

    SocketListener(const SocketListener& other) = delete;
    virtual ~SocketListener();

    /**
     * @brief Hand a valid socket to the listener
     * @param socket the socket
     * @throw std::runtime_error if there is already a socket assigned to this listener
     */
    void set_socket(std::unique_ptr<network::Socket> &&socket) throw(std::runtime_error);
    void set_socket(network::Socket* socket) throw(std::runtime_error);

    /**
     * @brief get the current or most recent fileno associated with this listener
     * @return -1 if there was never a valid Socket associated with this listener
     */
    int32_t get_fileno() const;

    /**
     * @brief will try to lock the associated mutex
     */
    bool try_lock();
    void lock();

    /**
     * @brief unlocks the associated mutex
     */
    void unlock();

    /**
     * Get the mutex associated with this object
     */
    std::mutex& mutex();

    /**
     * @brief update will be called when there is potentially new data for the socket
     * @note you have to handle this depending on what your socket does, e.g you might want to call accept() or receive()
     */
    virtual void update() = 0;

    /**
     * @brief returns true if the associated socket is connected
     */
    bool is_valid() const;

    const std::chrono::system_clock::time_point& last_update() const;

    /**
     * @brief mark socketlistener as updated
     * @note this should only be called by EventLoop
     */
    void mark_upated();

protected:
    network::Socket& socket()
    {
        return *m_socket;
    }

private:
    std::unique_ptr<network::Socket> m_socket;

    std::mutex m_mutex;

    int32_t m_fileno;

    std::chrono::system_clock::time_point m_last_update;
};

inline bool SocketListener::try_lock()
{
    return m_mutex.try_lock();
}

inline void SocketListener::lock()
{
    try {
        m_mutex.lock();
    } catch(const std::system_error &e) {
        LOG(FATAL) << e.what();
    }
}

inline void SocketListener::unlock()
{
    m_mutex.unlock();
}

inline void SocketListener::set_socket(network::Socket *socket) throw(std::runtime_error)
{
    set_socket(std::unique_ptr<network::Socket>(socket));
}

inline const std::chrono::system_clock::time_point& SocketListener::last_update() const
{
    return m_last_update;
}

inline void SocketListener::mark_upated()
{
    m_last_update = std::chrono::system_clock::now();
}

}

