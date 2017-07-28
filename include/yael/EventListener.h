#pragma once

#include <mutex>
#include <glog/logging.h>

namespace yael
{

class EventListener
{
public:
    // Modify reference count
    // Object will only be deleted if no other references are held
    // Note: this usually should only be called by the EventLoop itself
    uint16_t ref_count() const;
    void raise();
    void drop();

    virtual ~EventListener() {}

    virtual void update() = 0;

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
    uint16_t m_ref_count = 0;
};

inline bool EventListener::try_lock()
{
    return m_mutex.try_lock();
}

inline void EventListener::lock()
{
    try {
        m_mutex.lock();
    } catch(const std::system_error &e) {
        LOG(FATAL) << e.what();
    }
}

inline void EventListener::unlock()
{
    m_mutex.unlock();
}

inline std::mutex& EventListener::mutex()
{
    return m_mutex;
}

inline void EventListener::raise()
{
    m_ref_count += 1;
}

inline void EventListener::drop()
{
    m_ref_count -= 1;
}

inline uint16_t EventListener::ref_count() const
{
    return m_ref_count;
}

}
