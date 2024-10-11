#include "yael/EventLoop.h"
#include "yael/EventListener.h"

#include <unistd.h>
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

// this code assumes epoll is thread-safe
// see http://lkml.iu.edu/hypermail/linux/kernel/0602.3/1661.html

inline uint32_t get_flags(EventListener::Mode mode)
{
    if(mode == EventListener::Mode::ReadOnly)
    {
        return EPOLLIN | BASE_EPOLL_FLAGS;
    }
    else
    {
        return EPOLLIN | EPOLLOUT | BASE_EPOLL_FLAGS;
    }
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

void EventLoop::initialize(int32_t num_threads) noexcept
{
    if(m_instance != nullptr)
    {
        return;  // already initialized
    }

    m_instance = new EventLoop(num_threads);
    m_instance->run();
}

void EventLoop::destroy() noexcept
{
    if(m_instance == nullptr)
    {
        LOG(FATAL) << "Cannot destroy event loop: instance does not exist";
    }

    if(m_instance->m_okay)
    {
        LOG(FATAL) << "Cannot stop event loop: has to be stopped first!";
    }

    delete m_instance;
    m_instance = nullptr;
}

void EventLoop::stop() noexcept
{
    if(!m_okay)
    {
        return;//no-op
    }

    std::unique_lock lock(m_event_listeners_mutex);
    m_okay = false;

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

    if(!m_okay && nfds <= 0)
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

        // Let's try to continue here, if possible
        LOG(ERROR) << "epoll_wait() returned an error: " << strerror(errno) << " (errno=" << errno << ")";
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
            // Event loop was terminated; wake up next thread
            increment_semaphore(m_event_semaphore);
            return {nullptr, EventType::None};
        }
    }
    else
    {
        EventType type;
        auto flags = events[0].events;

        const bool has_read = (flags & EPOLLIN) != 0U;
        const bool has_write = (flags & EPOLLOUT) != 0U;
        const bool has_error = (flags & EPOLLERR) != 0U;

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
            LOG(FATAL) << "Invalid event flag";
        }

        return {reinterpret_cast<EventListenerPtr*>(ptr), type};
    }
}

void EventLoop::register_event_listener(EventListenerPtr listener)
 noexcept
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

    lock.unlock();

    listener->re_register(true);
}

void EventLoop::register_socket(int32_t fileno, EventListenerPtr *ptr, uint32_t flags, bool modify)
{
    struct epoll_event ev;
    ev.events = flags;
    ev.data.ptr = ptr;

    auto op = modify ? EPOLL_CTL_MOD : EPOLL_CTL_ADD;

    const int epoll_res = epoll_ctl(m_epoll_fd, op, fileno, &ev);
    if(epoll_res != 0)
    {
        LOG(ERROR) << "epoll_ctl() failed: " << strerror(errno);
    }
}

void EventLoop::notify_listener_mode_change(EventListenerPtr listener, EventListener::Mode mode, bool first_time)
    noexcept
{
    auto flags = get_flags(mode);
    EventListenerPtr *ptr = nullptr;

    {
        const std::unique_lock lock(m_event_listeners_mutex);
        auto it = m_event_listeners.find(listener->get_fileno());

        if(it == m_event_listeners.end())
        {
            // can happen during shut down
            LOG(WARNING) << "Failed to update listener mode: no such event listener";
            return;
        }

        ptr = it->second;
    }

    register_socket(listener->get_fileno(), ptr, flags, !first_time);
}

void EventLoop::unregister_event_listener(EventListenerPtr listener) noexcept
{
    const std::unique_lock lock(m_event_listeners_mutex);

    auto fileno = listener->get_fileno();
    auto it = m_event_listeners.find(fileno);

    if(it == m_event_listeners.end())
    {
        // ignore
    }
    else
    {
        // (except for when releasing the socket manually)
        const auto epoll_res = epoll_ctl(m_epoll_fd, EPOLL_CTL_DEL, fileno, nullptr);
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
        else if(type == EventType::Error)
        {
            listener->on_error();
        }
        else
        {
            LOG(FATAL) << "Invalid event type!";
        }

        listener->re_register(false);
    }
}

void EventLoop::run() noexcept
{
    auto num_threads = m_num_threads;

    if(num_threads <= 0)
    {
        num_threads = 2 * static_cast<int32_t>(std::thread::hardware_concurrency());

        if(num_threads <= 0)
        {
            LOG(FATAL) << "Could not detect number of hardware threads supported!";
        }
    }

    for(auto i = 0; i < num_threads; ++i)
    {
        m_threads.emplace_back(std::thread(&EventLoop::thread_loop, this));
    }
}

void EventLoop::wait() noexcept
{
    for(auto &t: m_threads)
    {
        t.join();
    }
}

}
