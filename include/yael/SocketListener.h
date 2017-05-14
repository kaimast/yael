#pragma once

#include <mutex>
#include <glog/logging.h>

namespace yael
{

class SocketListener
{
public:
    virtual ~SocketListener() {}

    virtual void update() = 0;
    virtual bool is_valid() const = 0;
    virtual int32_t get_fileno() const = 0;

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

private:
    std::mutex m_mutex;
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
