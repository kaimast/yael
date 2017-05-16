#pragma once

#include <thread>
#include <stack>
#include <unordered_map>
#include <mutex>
#include <vector>
#include <list>
#include <stdint.h>

namespace yael
{

class SocketListener;
class EventListener;

/**
 * @brief The main EventLoop class
 * @note This is a singleton. A process should only have one event loop.
 */
class EventLoop
{
public:
     /**
     * @brief initalizes worker threads that poll for new events
     */
    void run();

    /**
     * @brief wait for event loop to terminate
     */
    void wait();

    /**
     * @brief manually update the event loop
     *      This function will block until a new event occurs
     * @note only call this if you don't use run()
     */
    void update();

    /**
     * @brief Registers a new socket listener
     * @note this will be called by SocketListener automatically
     */
    void register_socket_listener(int32_t fileno, SocketListener *listener);

    void stop();

    bool is_okay() const;

    void register_time_event(uint64_t timeout, EventListener &listener);
    
    /**
     * @brief get the instance of the singleton
     * @throws a runtime_error if it hasn't been intialized yet
     */
    static EventLoop& get_instance() throw(std::runtime_error);

    /**
     * @brief Initializes the event loop
     * @param num_threads
     *		amount of threads. By default (-1) it will estimate based on CPU cores avaiable
     * @throws runtime_error if already initialized
     */
    static void initialize(int32_t num_threads = -1) throw(std::runtime_error);

    /**
     * @brief destroys the event loop instance. it is safe to call this multiple times.
     * @note you still have to destroy the instance after the event loop is destroyed
     */
    static void destroy();

    uint64_t get_time() const;

private:
    void thread_loop();

    void register_socket(int32_t fileno);

    EventListener* get_next_event();

    /**
     * Pull new events from epoll. This should only be called by get_next_event
     */
    void pull_more_events();

    /**
     * Constructor only called by initialize()
     */
    EventLoop(int32_t num_threads);
    ~EventLoop();

    static EventLoop* m_instance;

    bool m_okay;

    std::mutex m_epoll_mutex;
    std::mutex m_event_listeners_mutex;
    std::mutex m_queued_events_mutex;

    std::stack<std::thread> m_threads;

    std::list<EventListener*> m_queued_events;

    bool m_has_time_events;

    std::vector<std::pair<uint64_t, EventListener*>> m_time_events;
    std::unordered_map<int32_t, SocketListener*> m_socket_listeners;

    const int32_t m_epoll_fd;
    const int32_t m_num_threads;
};


inline EventLoop& EventLoop::get_instance() throw(std::runtime_error)
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


