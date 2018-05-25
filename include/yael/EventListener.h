#pragma once

#include <mutex>
#include <memory>
#include <glog/logging.h>

namespace yael
{

class EventListener : public std::enable_shared_from_this<EventListener>
{
public:
    virtual ~EventListener() = default;

    /**
     * Handle events
     */
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

    /**
     * Does the underlying socket have messages
     */
    virtual bool is_valid() const = 0;

    /** 
     * What is the socket's filedescriptor?
     * (used to identify this listener
     */
    virtual int32_t get_fileno() const = 0;

private:
    std::mutex m_mutex;
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

typedef std::shared_ptr<EventListener> EventListenerPtr;

}
