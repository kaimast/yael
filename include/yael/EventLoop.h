#pragma once

#include <thread>
#include <stack>
#include <unordered_map>
#include <mutex>
#include <vector>
#include <list>
#include <stdint.h>

#include "SocketListener.h"

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
     */
    void wait();

    /// Allocate a socket listener as a shared object
    template<typename T, typename... Args>
    std::shared_ptr<T> allocate_socket_listener(Args&&... args)
    {
        return std::make_shared<T>(std::forward<Args>(args)...);
    }

    // Same as allocate_* but also registers the listener
    template<typename T, typename... Args>
    std::shared_ptr<T> make_socket_listener(Args&&... args)
    {
        auto l = std::make_shared<T>(std::forward<Args>(args)...);
        this->register_socket_listener(std::dynamic_pointer_cast<SocketListener>(l));
        return l;
    }

    void stop();

    bool is_okay() const;

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

    uint64_t get_time() const;

    void register_socket_listener(SocketListenerPtr listener);

    void queue_event(std::shared_ptr<EventListener> l);

private:
    void run();
    
    EventListenerPtr update();
    
    void thread_loop();

    void register_socket(int32_t fileno, int32_t flags = -1);

    EventListenerPtr get_next_event();

    /**
     * Constructor only called by initialize()
     */
    EventLoop(int32_t num_threads);
    ~EventLoop();

    static EventLoop* m_instance;

    bool m_okay;

    std::mutex m_event_listeners_mutex;
    std::mutex m_queued_events_mutex;

    std::list<std::thread> m_threads;

    std::list<EventListenerPtr> m_queued_events;

    bool m_has_time_events;

    std::vector<std::pair<uint64_t, EventListenerPtr>> m_time_events;
    std::unordered_map<int32_t, SocketListenerPtr> m_socket_listeners;

    const int32_t m_epoll_fd;
    const int32_t m_event_semaphore;
    const int32_t m_num_threads;
};


inline EventLoop& EventLoop::get_instance()
{
    if(!m_instance)
        throw std::runtime_error("Event loop not initialized (yet)!");

    return *m_instance;
}

inline bool EventLoop::is_okay() const
{
    return m_okay;
}

}


