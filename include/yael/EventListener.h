#pragma once

#include <mutex>
#include <memory>
#include <atomic>
#include <glog/logging.h>

namespace yael
{

class EventListener : public std::enable_shared_from_this<EventListener>
{
public:
    enum class Mode
    {
        ReadOnly,
        ReadWrite
    };

    virtual ~EventListener() = default;

    /**
     * Handle events
     */
    virtual void on_read_ready() = 0;
    virtual void on_write_ready() = 0;

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
     * Is the underlying socket still valid
     */
    virtual bool is_valid() const = 0;

    /** 
     * What is the socket's filedescriptor?
     * (used to identify this listener
     */
    virtual int32_t get_fileno() const = 0;

    Mode mode() const { return m_mode; }

    void set_mode(Mode mode);

    virtual void close_socket() = 0;

protected:
    EventListener(Mode mode)
        : m_mode(mode)
    {}

private:
    std::mutex m_mutex;
    std::atomic<Mode> m_mode;
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
