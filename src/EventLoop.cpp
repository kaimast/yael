#include "yael/EventLoop.h"
#include "yael/SocketListener.h"

#include <glog/logging.h>
#include <assert.h>
#include <chrono>
#include <sys/eventfd.h>
#include <sys/epoll.h>

// How many events can one thread see at most
// Increasing this value might make the scheduler fairer but also slower
constexpr int32_t MAX_EVENTS = 1;
constexpr int32_t TIMEOUT_MAX = 100;
constexpr int32_t EPOLL_FLAGS = EPOLLIN | EPOLLERR | EPOLLRDHUP | EPOLLET;

using namespace yael;

EventLoop::EventLoop(int32_t num_threads)
    : m_okay(true), m_has_time_events(false), m_epoll_fd(epoll_create1(0)),
      m_event_semaphore(eventfd(0, EFD_NONBLOCK | EFD_SEMAPHORE)), m_num_threads(num_threads)
{
    if(m_epoll_fd < 0)
        LOG(FATAL) << "epoll_create1() failed: " << strerror(errno);

    register_socket(m_event_semaphore, EPOLLIN | EPOLLET);
}

EventLoop::~EventLoop()
{
    ::close(m_epoll_fd);
}

EventLoop* EventLoop::m_instance = nullptr;

void EventLoop::initialize(int32_t num_threads) 
{
    if(m_instance)
        return;  // already initialized

    m_instance = new EventLoop(num_threads);
    m_instance->run();
}

void EventLoop::destroy()
{
    delete m_instance;
    m_instance = nullptr;
}

void EventLoop::stop()
{
    m_okay = false;
}

void EventLoop::register_time_event(uint64_t timeout, EventListenerPtr listener)
{
    std::lock_guard<std::mutex> lock_guard(m_event_listeners_mutex);

    m_has_time_events = true;
    auto start = get_time() + timeout;

    for(auto it = m_time_events.begin(); it != m_time_events.end(); ++it)
    {
        if(it->first >= start)
        {
            m_time_events.emplace(it, std::pair<uint64_t, EventListenerPtr>{start, listener});
            return;
        }
    }

    m_time_events.emplace_back(std::pair<uint64_t, EventListenerPtr>{start, listener});
}

EventListenerPtr EventLoop::get_next_event()
{
    std::lock_guard<std::mutex> lock_guard(m_queued_events_mutex);

    uint64_t buffer = 0;
    auto i = read(m_event_semaphore, reinterpret_cast<uint8_t*>(&buffer), sizeof(buffer));
    if(i <= 0)
        return nullptr;
   
    auto it = m_queued_events.begin();
    auto listener = *it;
    m_queued_events.erase(it);
    return listener;
}

uint64_t EventLoop::get_time() const
{
    using std::chrono::steady_clock;

    auto res = steady_clock::now();
    return std::chrono::duration_cast<std::chrono::milliseconds>(res.time_since_epoch()).count();
}

EventListenerPtr EventLoop::update()
{
    auto l = get_next_event();

    if(l)
        return l;

    epoll_event events[MAX_EVENTS];

    int32_t timeout = TIMEOUT_MAX;
    if(m_has_time_events)
    {
        auto it = m_time_events.begin();//FIXME lock time events
        const auto current_time = get_time();
        if(current_time >= it->first)
            timeout = 0;
        else
            timeout = static_cast<int32_t>(it->first - current_time);
    }
    
    int nfds = -1;
    while(nfds < 0 || (nfds == 0 && !m_has_time_events && m_okay))
        nfds = epoll_wait(m_epoll_fd, events, MAX_EVENTS, timeout);

    if(!m_okay) // Event loop was terminated
        return {nullptr};

    if(nfds < 0)
    {
        // was interrupted by a signal. ignore
        // badf means the content server is shutting down
        if(errno == EINTR || errno == EBADF)
        {
            stop();
            return {nullptr};
        }

        LOG(FATAL) << "epoll_wait() returned an error: " << strerror(errno);
    }

    m_event_listeners_mutex.lock();
    
    for(int32_t i = 0; i < nfds; ++i)
    {
        auto fd = events[i].data.fd;
        if(fd == m_event_semaphore)
        {
            if(m_queued_events.size() > 1 || nfds > 1)
            {
                // wake up another thread to handle this
                auto res = eventfd_write(m_event_semaphore, 0);

                if(res < 0)
                    throw std::runtime_error("eventfd error");
            }
            
            continue; //do nothing... event already queued
        }

        auto it = m_socket_listeners.find(fd);

        if(it == m_socket_listeners.end())
        {
            DLOG(INFO) << "Discarded event as socket listner doesn't exist";
            continue; // might just have been closed...
        }

        auto listener = it->second;
        queue_event(listener);
   }

    if(m_time_events.size() > 0)
    {
        auto current_time = get_time();
        auto it = m_time_events.begin();
        
        while(it != m_time_events.end())
        {
            auto start = it->first;

            if(start <= current_time)
            {
                auto e = it->second;
                queue_event(e);
                it = m_time_events.erase(it);

                if(m_time_events.size() == 0)
                    m_has_time_events = false;
            }
            else
                break;
        }
    }

    m_event_listeners_mutex.unlock();

    return update();
}

void EventLoop::queue_event(std::shared_ptr<EventListener> l)
{
    std::lock_guard<std::mutex> lock_guard(m_queued_events_mutex);
    m_queued_events.push_back(l);
     
    auto res = eventfd_write(m_event_semaphore, 1);

    if(res < 0)
        throw std::runtime_error("Eventfd error");
}

void EventLoop::register_socket(int32_t fileno, int32_t flags)
{
    if(fileno <= 0)
        throw std::runtime_error("Not a valid socket");

    struct epoll_event ev;
    ev.events = flags < 0 ? EPOLL_FLAGS : flags;
    ev.data.fd = fileno;

    int res = epoll_ctl(m_epoll_fd, EPOLL_CTL_ADD, fileno, &ev);
    if(res != 0)
        LOG(FATAL) << "epoll_ctl() failed: " << strerror(errno);
}

void EventLoop::register_socket_listener(SocketListenerPtr listener)
{
    auto fileno = listener->get_fileno();

    register_socket(fileno);
    
    std::lock_guard<std::mutex> lock_guard(m_event_listeners_mutex);
    m_socket_listeners[fileno] = listener;
}

void EventLoop::thread_loop()
{
    while(this->is_okay())
    {
        auto listener = update();

        if(!listener)
            return; // terminate

        listener->lock();
        listener->update();

        auto slistener = std::dynamic_pointer_cast<SocketListener>(listener);
        
        if(slistener && !slistener->is_valid())
        {
            m_event_listeners_mutex.lock();
            auto it = m_socket_listeners.find(slistener->get_fileno());
            if(it != m_socket_listeners.end())
                m_socket_listeners.erase(it);
            m_event_listeners_mutex.unlock();
        }
        else
        {
            listener->unlock();
        }
    }
}

void EventLoop::run()
{
    int32_t num_threads = m_num_threads;

    if(num_threads <= 0)
    {
        num_threads = 2 * std::thread::hardware_concurrency();

        if(num_threads <= 0)
            throw std::runtime_error("Could not detect number of hardware threads supported!");
    }

    for(int32_t i = 0; i < num_threads; ++i)
    {
        m_threads.push(std::thread(&EventLoop::thread_loop, this));
    }
}

void EventLoop::wait()
{
    while(!m_threads.empty())
    {
        m_threads.top().join();
        m_threads.pop();
    }
}
