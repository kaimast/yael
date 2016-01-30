#pragma once

#include <mutex>
#include <memory>
#include <glog/logging.h>

#include "network/Socket.h"

namespace yael
{

class EventLoop;

/**
 * @brief The SocketListener class
 * @note
 */
class SocketListener
{
public:
    /**
     * @brief Construct with a valid socket
     */
    SocketListener(EventLoop &loop, std::unique_ptr<network::Socket> &&socket);

    /**
     * @brief Construct without a valid socket (yet)
     */
    SocketListener(EventLoop &loop);

    SocketListener(const SocketListener& other) = delete;
    virtual ~SocketListener();

    /**
     * @brief Hand a valid socket to the listener
     * @param socket the socket
     * @throw std::runtime_error if there is already a socket assigned to this listener
     */
    void set_socket(std::unique_ptr<network::Socket> &&socket) throw(std::runtime_error);

    /**
     * @brief get the current or most recent fileno associated with this listener
     * @return -1 if there was never a valid Socket associated with this listener
     */
    int32_t get_fileno() const;

    bool try_lock();
    void lock();
    void unlock();

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

private:
    EventLoop &m_loop;

    std::mutex m_mutex;
    std::unique_ptr<network::Socket> m_socket;

    int32_t m_fileno;
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

}

