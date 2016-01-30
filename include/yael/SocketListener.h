#pragma once

#include <mutex>
#include <glog/logging.h>

namespace yael
{

//! Components that listen for incoming messages
class SocketListener
{
public:
    SocketListener();

    SocketListener(const SocketListener& other) = delete;
    virtual ~SocketListener() {}

    //! There is new data to read
    //! Returns if socket listener was closed.
    virtual bool update() = 0;

    virtual int32_t get_fileno() const = 0;

    bool try_lock();
    void lock();
    void unlock();

protected:
    std::mutex m_mutex;
};

inline SocketListener::SocketListener() : m_mutex()
{}

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

