#include "yael/EventLoop.h"
#include "yael/EventListener.h"

#include <glog/logging.h>
#include <cassert>
#include <chrono>
#include <sys/eventfd.h>
#include <sys/epoll.h>

namespace yael
{

/// A thread can handle at most one event
constexpr int32_t MAX_EVENTS = 1;

const uint32_t BASE_EPOLL_FLAGS = EPOLLERR | EPOLLRDHUP | EPOLLONESHOT;

inline uint32_t get_flags(EventListener::Mode mode)
{
    uint32_t flags;
    if(mode == EventListener::Mode::ReadOnly)
    {
        flags = EPOLLIN | BASE_EPOLL_FLAGS;
    }
    else
    {
        flags = EPOLLIN | EPOLLOUT | BASE_EPOLL_FLAGS;
    }

    return flags;
}

inline void increment_semaphore(int32_t fd)
{
    constexpr uint64_t SEMAPHORE_INC = 1;

    auto res = write(fd, &SEMAPHORE_INC, sizeof(SEMAPHORE_INC));
    if(res != sizeof(SEMAPHORE_INC))
    {
        LOG(FATAL) << "eventfd write failed";
    }
}

inline void decrement_semaphore(int32_t fd)
{
    uint64_t val = 0;

    auto res = read(fd, &val, sizeof(val));

    if(res != sizeof(val) || val == 0)
    {
        LOG(FATAL) << "Invalid state";
    }
}

EventLoop::EventLoop(int32_t num_threads)
    : m_okay(true), m_epoll_fd(epoll_create1(0)),
      m_event_semaphore(eventfd(0, EFD_NONBLOCK | EFD_SEMAPHORE)), m_num_threads(num_threads)
{
    if(m_epoll_fd < 0)
    {
        LOG(FATAL) << "epoll_create1() failed: " << strerror(errno);
    }

    register_socket(m_event_semaphore, nullptr, EPOLLIN | EPOLLET, false);
}

EventLoop::~EventLoop()
{
    ::close(m_epoll_fd);
}

EventLoop* EventLoop::m_instance = nullptr;

void EventLoop::initialize(int32_t num_threads) 
{
    if(m_instance != nullptr)
    {
        return;  // already initialized
    }

    m_instance = new EventLoop(num_threads);
    m_instance->run();
}

void EventLoop::destroy()
{
    if(m_instance == nullptr)
    {
        throw std::runtime_error("Instance does not exist.");
    }

    if(m_instance->m_okay)
    {
        throw std::runtime_error("Event loop has to be stopped first!");
    }

    delete m_instance;
    m_instance = nullptr;
}

void EventLoop::stop()
{
    if(!m_okay)
    {
        return;//no-op
    }
    
    std::unique_lock lock(m_event_listeners_mutex);

    while(!m_event_listeners.empty())
    {
        auto it = m_event_listeners.begin();
        auto [fileno, ptr] = *it;

        (void)fileno;
        auto listener = *ptr;

        lock.unlock();
        listener->close_socket();
        lock.lock();
    }

    while(!m_event_listeners.empty())
    {
        m_event_listeners_cond.wait(lock);
    }

    m_okay = false;
    increment_semaphore(m_event_semaphore);
}

uint64_t EventLoop::get_time() const
{
    using std::chrono::steady_clock;

    auto res = steady_clock::now();
    return std::chrono::duration_cast<std::chrono::milliseconds>(res.time_since_epoch()).count();
}

std::pair<EventListenerPtr*, EventLoop::EventType> EventLoop::update()
{
    epoll_event events[MAX_EVENTS];
    int nfds = -1;
    const int32_t timeout = -1;

    while(m_okay && nfds < 0)
    {
        nfds = epoll_wait(m_epoll_fd, events, MAX_EVENTS, timeout);
    }

    if(!m_okay && nfds == 0)
    {
        // Event loop was terminated; wakeup next thread
        increment_semaphore(m_event_semaphore);
        return {nullptr, EventType::None};
    }

    if(nfds < 0)
    {
        // was interrupted by a signal. ignore
        // badf means the content server is shutting down
        if(errno == EINTR || errno == EBADF)
        {
            stop();
            return {nullptr, EventType::None};
        }

        LOG(FATAL) << "epoll_wait() returned an error: " << strerror(errno);
    }

    if(nfds > 1)
    {
        LOG(FATAL) << "Invalid state: got more than one event";
    }

    auto ptr = events[0].data.ptr;

    if(ptr == nullptr) // it's the event semaphore
    {
        // Consume it so the event fd doesn't overflow
        decrement_semaphore(m_event_semaphore);

        if(m_okay)
        {
            DLOG(WARNING) << "Spurious wakeup";
            return update();
        }
        else
        {
            // Event loop was terminated; wakeup next thrad
            increment_semaphore(m_event_semaphore);
            return {nullptr, EventType::None};
        }
    }
    else
    {
        EventType type;
        auto flags = events[0].events;

        bool has_read = (flags & EPOLLIN) != 0u;
        bool has_write = (flags & EPOLLOUT) != 0u;
        bool has_error = (flags & EPOLLERR) != 0u;

        if(has_read && has_write)
        {
            type = EventType::ReadWrite;
        }
        else if(has_read)
        {
            type = EventType::Read;
        }
        else if(has_write)
        {
            type = EventType::Write;
        }
        else if(has_error)
        {
            type = EventType::Error;
        }
        else 
        {
            LOG(FATAL) << "invalid event flag";
        }

        return {reinterpret_cast<EventListenerPtr*>(ptr), type};
    }
}

void EventLoop::register_event_listener(EventListenerPtr listener)
{
    std::unique_lock lock(m_event_listeners_mutex);

    auto idx = listener->get_fileno();
    auto ptr = new EventListenerPtr(listener);

    while(true)
    {
        auto res = m_event_listeners.emplace(idx, ptr);
        
        if(res.second)
        {
            break;
        }
        else
        {
            // wait for other thread to process old event listener disconnect
            m_event_listeners_cond.wait(lock);
        }
    }

    auto fileno = listener->get_fileno();

    if(fileno <= 0)
    {
        throw std::runtime_error("Not a valid socket");
    }

    auto flags = get_flags(listener->mode());
    register_socket(fileno, ptr, flags);
}

void EventLoop::register_socket(int32_t fileno, EventListenerPtr *ptr, uint32_t flags, bool modify)
{
    struct epoll_event ev;
    ev.events = flags; 
    ev.data.ptr = ptr;

    auto op = modify ? EPOLL_CTL_MOD : EPOLL_CTL_ADD;

    int epoll_res = epoll_ctl(m_epoll_fd, op, fileno, &ev);
    if(epoll_res != 0)
    {
        LOG(ERROR) << "epoll_ctl() failed: " << strerror(errno);
    }
}

void EventLoop::notify_listener_mode_change(EventListenerPtr listener)
{
    auto flags = get_flags(listener->mode());
    EventListenerPtr *ptr = nullptr;

    {
        std::unique_lock lock(m_event_listeners_mutex);
        auto it = m_event_listeners.find(listener->get_fileno());

        if(it == m_event_listeners.end())
        {
            // can happen during shut down
            LOG(WARNING) << "Failed to update listener mode: no such event listener";
            return;
        }

        ptr = it->second;
    }

    register_socket(listener->get_fileno(), ptr, flags, true);
}

void EventLoop::unregister_event_listener(EventListenerPtr listener)
{
    std::unique_lock lock(m_event_listeners_mutex);

    if(listener->is_valid())
    {
        LOG(FATAL) << "Cannot unregister event listener while still valid";
    }

    auto fileno = listener->get_fileno();
    auto it = m_event_listeners.find(fileno);

    if(it == m_event_listeners.end())
    {
        // ignore
    }
    else
    {
        // (except for when releasing the socket manually)
        int epoll_res = epoll_ctl(m_epoll_fd, EPOLL_CTL_DEL, fileno, nullptr);
        (void)epoll_res; 
        
        delete it->second;
        m_event_listeners.erase(it);

        m_event_listeners_cond.notify_all();
    }
}

void EventLoop::thread_loop()
{
    while(this->is_okay())
    {
        auto res = update();
        auto &[ptr, type] = res;

        if(ptr == nullptr)
        {
            // terminate
            return;
        }

        // make a copy here
        auto listener = *ptr;

        if(type == EventType::ReadWrite)
        {
            listener->on_read_ready();
            listener->on_write_ready();
        }
        else if(type == EventType::Read)
        {
            listener->on_read_ready();
        }
        else if(type == EventType::Write)
        {
            listener->on_write_ready();
        }
        else
        {
            LOG(FATAL) << "invalid event type!";
        }

        uint32_t flags;
        if(listener->mode() == EventListener::Mode::ReadOnly)
        {
            flags = EPOLLIN | BASE_EPOLL_FLAGS;
        }
        else
        {
            flags = EPOLLIN | EPOLLOUT | BASE_EPOLL_FLAGS;
        }

        if(listener->is_valid())
        {
            register_socket(listener->get_fileno(), ptr, flags, true);
        }
        else
        {
            unregister_event_listener(listener);
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
        {
            throw std::runtime_error("Could not detect number of hardware threads supported!");
        }
    }

    for(int32_t i = 0; i < num_threads; ++i)
    {
        m_threads.emplace_back(std::thread(&EventLoop::thread_loop, this));
    }
}

void EventLoop::wait()
{
    for(auto &t: m_threads)
    {
        t.join();
    }
}

}
