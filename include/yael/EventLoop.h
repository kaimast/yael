#pragma once

#include <thread>
#include <stack>
#include <unordered_map>
#include <shared_mutex>
#include <vector>
#include <atomic>
#include <list>
#include <stdint.h>

#include "EventListener.h"

namespace yael
{

/**
 * @brief The main EventLoop class
 * @note This is a singleton. A process should only have one event loop.
 */
class EventLoop
{
public:
    /**
     * @brief wait for event loop to terminate
     *
     * @note this may only be called by at most one thread
     */
    void wait();

    template<typename T, typename... Args>
    std::shared_ptr<T> allocate_event_listener(Args&&... args)
    {
        return std::make_shared<T>(std::forward<Args>(args)...);
    }
    
    template<typename T, typename... Args>
    std::shared_ptr<T> make_event_listener(Args&&... args)
    {
        auto l = std::make_shared<T>(std::forward<Args>(args)...);
        auto sl = std::dynamic_pointer_cast<EventListener>(l);

        this->register_event_listener(sl);
        return l;
    }

    void register_event_listener(EventListenerPtr sl);

    /**
     * Shut the event loop down
     *
     * Note, this is non-blocking
     */
    void stop();

    /**
     * Is the event loop running and not about to be shut down?
     */
    bool is_okay() const;

    /**
     * @brief get the instance of the singleton
     * @throws a runtime_error if it hasn't been intialized yet
     */
    static EventLoop& get_instance();

    /**
     * @brief Initializes the event loop
     * @param num_threads
     *		amount of threads. By default (-1) it will estimate based on CPU cores avaiable
     * @throws runtime_error if already initialized
     */
    static void initialize(int32_t num_threads = -1);

    /**
     * @brief destroys the event loop instance. it is safe to call this multiple times.
     * @note you still have to destroy the instance after the event loop is destroyed
     */
    static void destroy();

    /**
     * Get relative local time (in milliseconds)
     */
    uint64_t get_time() const;

    /**
     * Unregister an event listener
     * This will mark the reference to the listener to be removed once all events have been processed
     * 
     * Note: you must hold the lock to listener before calling this function
     *
     * @param purge [optional]
     *      if set to true it will discard all pending events
     */
    void unregister_event_listener(EventListenerPtr listener, bool purge = false);

    static bool is_initialized() 
    {
        return m_instance != nullptr;
    }

private:
    void run();
    
    EventListener* update();
    
    void thread_loop();

    void register_socket(int32_t fileno, void *ptr, int32_t flags = -1);
    void unregister_socket(int32_t fileno);

    /**
     * Constructor only called by initialize()
     */
    EventLoop(int32_t num_threads);
    ~EventLoop();

    static EventLoop* m_instance;

    std::atomic<bool> m_okay;

    std::mutex m_queued_events_mutex;

    std::list<std::thread> m_threads;

    std::shared_mutex m_event_listeners_mutex;
    std::unordered_map<uintptr_t, EventListenerPtr> m_event_listeners;

    const int32_t m_epoll_fd;
    const int32_t m_event_semaphore;
    const int32_t m_num_threads;
};


inline EventLoop& EventLoop::get_instance()
{
    if(!m_instance)
    {
        throw std::runtime_error("Event loop not initialized (yet)!");
    }

    return *m_instance;
}

inline bool EventLoop::is_okay() const
{
    return m_okay;
}

}


