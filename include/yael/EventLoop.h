#pragma once

#include <thread>
#include <stack>
#include <unordered_map>
#include <shared_mutex>
#include <vector>
#include <list>
#include <stdint.h>
#include <boost/lockfree/queue.hpp>

#include "SocketListener.h"

namespace yael
{

class EventListenerHandle;

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
        this->register_event_listener(std::dynamic_pointer_cast<EventListener>(l));
        return l;
    }

    template<typename T, typename... Args>
    std::shared_ptr<T> make_socket_listener(Args&&... args)
    {
        auto l = std::make_shared<T>(std::forward<Args>(args)...);
        auto sl = std::dynamic_pointer_cast<SocketListener>(l);

        this->register_socket_listener(sl);
        return l;
    }

    void register_socket_listener(SocketListenerPtr sl)
    {
        this->register_event_listener(sl);
        this->register_socket(sl->get_fileno(), sl.get());
    }

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
     * Scheduled event listener to be called in <timeout> ms
     */
    void register_time_event(uint64_t timeout, EventListenerPtr listener);
    
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

    void register_event_listener(EventListenerPtr listener);

    /**
     * Unregister an event listener
     * This will mark the reference to the listener to be removed once all events have been processed
     * 
     * Note: you must hold the lock to listener before calling this function
     */
    void unregister_event_listener(EventListenerPtr listener);

    void unregister_socket_listener(SocketListenerPtr listener)
    {
        this->unregister_socket(listener->get_fileno());
        this->unregister_event_listener(listener);
    }

    void queue_event(EventListener &l);

    static bool is_initialized() 
    {
        return m_instance != nullptr;
    }

private:
    void run();
    
    EventListenerHandle* update();
    
    void thread_loop();

    void register_socket(int32_t fileno, void *ptr, int32_t flags = -1);
    void unregister_socket(int32_t fileno);

    EventListenerHandle* get_next_event();

    /**
     * Constructor only called by initialize()
     */
    EventLoop(int32_t num_threads);
    ~EventLoop();

    static EventLoop* m_instance;

    std::atomic<bool> m_okay;

    std::mutex m_queued_events_mutex;

    std::list<std::thread> m_threads;

    boost::lockfree::queue<EventListenerHandle*> m_queued_events;
    std::atomic<size_t> m_num_queued_events;

    std::atomic<bool> m_has_time_events;

    std::mutex m_time_events_mutex;
    std::list<std::pair<uint64_t, EventListenerHandle*>> m_time_events;

    std::shared_mutex m_event_listeners_mutex;
    std::unordered_map<uintptr_t, EventListenerHandle*> m_event_listeners;

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


