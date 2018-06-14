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
    std::unique_lock lock(m_event_listeners_mutex);

    for(auto &[fileno, ptr] : m_event_listeners)
    {
        (void)fileno;
        delete ptr;
    }

    m_event_listeners.clear();
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

    m_okay = false;
    increment_semaphore(m_event_semaphore);
}

uint64_t EventLoop::get_time() const
{
    using std::chrono::steady_clock;

    auto res = steady_clock::now();
    return std::chrono::duration_cast<std::chrono::milliseconds>(res.time_since_epoch()).count();
}

EventListenerPtr* EventLoop::update()
{
    epoll_event events[MAX_EVENTS];
    int nfds = -1;
    const int32_t timeout = -1;

    while(m_okay && nfds < 0)
    {
        nfds = epoll_wait(m_epoll_fd, events, MAX_EVENTS, timeout);
    }

    if(!m_okay)
    {
        // Event loop was terminated
        // Consume and wake up next thread
        decrement_semaphore(m_event_semaphore);
        increment_semaphore(m_event_semaphore);

        return nullptr;
    }

    if(nfds < 0)
    {
        // was interrupted by a signal. ignore
        // badf means the content server is shutting down
        if(errno == EINTR || errno == EBADF)
        {
            stop();
            return nullptr;
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
        eventfd_t val;
        eventfd_read(m_event_semaphore, &val);

        DLOG(WARNING) << "Spurious wakeup";
        return update();
    }
    else
    {
        return reinterpret_cast<EventListenerPtr*>(ptr);
    }
}

void EventLoop::register_event_listener(EventListenerPtr listener)
{
    std::unique_lock lock(m_event_listeners_mutex);
    auto idx = listener->get_fileno();
    auto ptr = new EventListenerPtr(listener);
    /*auto res = m_event_listeners.emplace(idx, ptr);

    if(!res.second)
    {
        LOG(FATAL) << "Listener #" << idx << " already registered";
    } FIXME */

    m_event_listeners[idx] = ptr;

    auto fileno = listener->get_fileno();

    if(fileno <= 0)
    {
        throw std::runtime_error("Not a valid socket");
    }

    register_socket(fileno, ptr);
}

void EventLoop::register_socket(int32_t fileno, EventListenerPtr *ptr, uint32_t flags, bool modify)
{
    constexpr uint32_t DEFAULT_EPOLL_FLAGS = EPOLLIN | EPOLLERR | EPOLLRDHUP | EPOLLONESHOT;

    struct epoll_event ev;
    ev.events = flags == 0 ? DEFAULT_EPOLL_FLAGS : flags;
    ev.data.ptr = ptr;

    auto op = modify ? EPOLL_CTL_MOD : EPOLL_CTL_ADD;

    int epoll_res = epoll_ctl(m_epoll_fd, op, fileno, &ev);
    if(epoll_res != 0)
    {
        LOG(FATAL) << "epoll_ctl() failed: " << strerror(errno);
    }
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
        LOG(WARNING) << "Event listener #" << fileno << " already unregistered";
    }
    else
    {
        // (except for when releasing the socket manually)
        int epoll_res = epoll_ctl(m_epoll_fd, EPOLL_CTL_DEL, fileno, nullptr);
        (void)epoll_res; 
        
        delete it->second;
        m_event_listeners.erase(it);
    }
}

void EventLoop::thread_loop()
{
    while(this->is_okay())
    {
        auto ptr = update();
        if(ptr == nullptr)
        {
            // terminate
            return;
        }

        auto listener = *ptr;

        listener->lock();
        listener->update();

        if(listener->is_valid())
        {
            // oneshot ensures only one thread processes this handler
            register_socket(listener->get_fileno(), ptr, 0, true);
        }
        else
        {
            unregister_event_listener(listener);
        }

        listener->unlock();
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
