#pragma once

#include <thread>
#include <unordered_map>
#include <shared_mutex>
#include <condition_variable>
#include <atomic>
#include <list>
#include <cstdint>

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
    EventLoop(const EventLoop &other) = delete;

    /**
     * @brief wait for event loop to terminate
     *
     * @note this may only be called by at most one thread
     */
    void wait() noexcept;

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

    void register_event_listener(EventListenerPtr listener) noexcept;

    /**
     * Shut the event loop down. This will stop all active event listeners
     * Note that this must be called from outside an event listener to avoid a deadlock!
     */
    void stop() noexcept;

    /**
     * Is the event loop running and not about to be shut down?
     */
    bool is_okay() const noexcept;

    /**
     * @brief get the instance of the singleton
     * @throws a runtime_error if it hasn't been intialized yet
     */
    static EventLoop& get_instance();

    /**
     * @brief Initializes the event loop
     * @param num_threads
     *		amount of threads. By default (-1) it will estimate based on CPU cores avaiable
     */
    static void initialize(int32_t num_threads = -1) noexcept;

    /**
     * @brief destroys the event loop instance. it is safe to call this multiple times.
     * @note you still have to destroy the instance after the event loop is destroyed
     */
    static void destroy() noexcept;

    /**
     * Get relative local time (in milliseconds)
     */
    uint64_t get_time() const;

    void notify_listener_mode_change(EventListenerPtr listener, EventListener::Mode mode, bool first_time) noexcept;
    void unregister_event_listener(EventListenerPtr listener) noexcept;

    static bool is_initialized() noexcept
    {
        return m_instance != nullptr;
    }

private:
    explicit EventLoop(int32_t num_threads);
    ~EventLoop();

    void run() noexcept;
    
    enum EventType
    {
        None,
        Read,
        Write,
        ReadWrite,
        Error
    };
    
    std::pair<EventListenerPtr, EventType> update();
    
    void thread_loop();

    void register_socket(int32_t fileno, uint32_t flags, bool modify = false);

    void unregister_socket(int32_t fileno);

    static EventLoop* m_instance;

    std::atomic<bool> m_okay;

    std::list<std::thread> m_threads;

    std::shared_mutex m_event_listeners_mutex;
    std::condition_variable_any m_event_listeners_cond;

    /// Mapping from the filedescriptor to the event listener
    std::unordered_map<int32_t, EventListenerPtr> m_event_listeners;

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

inline bool EventLoop::is_okay() const noexcept
{
    return m_okay;
}

}


