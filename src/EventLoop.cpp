#include "yael/EventLoop.h"
#include "yael/SocketListener.h"

#include <glog/logging.h>
#include <cassert>
#include <chrono>
#include <sys/eventfd.h>
#include <sys/epoll.h>

namespace yael
{

// How many events can one thread see at most
// Increasing this value might make the scheduler fairer but also slower
constexpr int32_t MAX_EVENTS = 1;
constexpr int32_t TIMEOUT_MAX = 100;
constexpr int32_t EPOLL_FLAGS = EPOLLIN | EPOLLERR | EPOLLRDHUP | EPOLLET;

/// Lockless queue doesn't like shared pointers so this extra wrapper is needea

struct EventListenerHandle
{
    uintptr_t index;

    std::atomic<bool> active;
    std::atomic<uint16_t> num_events;

    std::shared_ptr<EventListener> pointer;
};

EventLoop::EventLoop(int32_t num_threads)
    : m_okay(true), m_queued_events(0), m_has_time_events(false), m_epoll_fd(epoll_create1(0)),
      m_event_semaphore(eventfd(0, EFD_NONBLOCK | EFD_SEMAPHORE)), m_num_threads(num_threads)
{
    if(m_epoll_fd < 0)
    {
        LOG(FATAL) << "epoll_create1() failed: " << strerror(errno);
    }

    register_socket(m_event_semaphore, nullptr, EPOLLIN | EPOLLET);
}

EventLoop::~EventLoop()
{
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

    // Wake up epoll
    for(uint32_t _ = 0; _ < m_threads.size(); ++_)
    {
        // wake up thread by writing to eventfd
        eventfd_write(m_event_semaphore, 0);
    }
}

void EventLoop::register_time_event(uint64_t timeout, EventListenerPtr listener)
{
    std::shared_lock lock_guard(m_event_listeners_mutex);

    auto idx = reinterpret_cast<uintptr_t>(listener.get());
    auto it = m_event_listeners.find(idx);

    if(it == m_event_listeners.end())
    {
        LOG(FATAL) << "No such time event";
    }

    auto hdl = it->second;
    hdl->num_events++;

    m_has_time_events = true;
    auto start = get_time() + timeout;

    for(auto it = m_time_events.begin(); it != m_time_events.end(); ++it)
    {
        if(it->first >= start)
        {
            m_time_events.emplace(it, std::pair<uint64_t, EventListenerHandle*>{start, hdl});
            return;
        }
    }

    m_time_events.emplace_back(std::pair<uint64_t, EventListenerHandle*>{start, hdl});
}

EventListenerHandle* EventLoop::get_next_event()
{
    uint64_t buffer = 0;
    auto i = read(m_event_semaphore, reinterpret_cast<uint8_t*>(&buffer), sizeof(buffer));

    if(i <= 0)
    {
        return nullptr;
    }

    EventListenerHandle *hdl = nullptr;

    bool res = m_queued_events.pop(hdl);

    if(!res || hdl->num_events < 1)
    {
        throw std::runtime_error("invalid state!");
    }
    
    m_num_queued_events--;
    return hdl;
}

uint64_t EventLoop::get_time() const
{
    using std::chrono::steady_clock;

    auto res = steady_clock::now();
    return std::chrono::duration_cast<std::chrono::milliseconds>(res.time_since_epoch()).count();
}

EventListenerHandle* EventLoop::update()
{
    auto l = get_next_event();

    if(l != nullptr)
    {
        return l;
    }

    epoll_event events[MAX_EVENTS];
    int nfds = -1;

    int32_t timeout = TIMEOUT_MAX;
    if(m_has_time_events)
    {
        auto it = m_time_events.begin();//FIXME lock time events
        const auto current_time = get_time();
        if(current_time >= it->first)
        {
            timeout = 0;
        }
        else
        {
            timeout = static_cast<int32_t>(it->first - current_time);
        }
    }
    
    while(nfds < 0 || (nfds == 0 && !m_has_time_events && m_okay))
    {
        nfds = epoll_wait(m_epoll_fd, events, MAX_EVENTS, timeout);
    }

    if(!m_okay)
    {
        // Event loop was terminated
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

   
    for(int32_t i = 0; i < nfds; ++i)
    {
        auto ptr = events[i].data.ptr;

        if(ptr == nullptr) // it's the event semaphore
        {
            if(m_num_queued_events > 1 || nfds > 1)
            {
                // wake up another thread to handle this
                auto res = eventfd_write(m_event_semaphore, 0);

                if(res < 0)
                {
                    throw std::runtime_error("eventfd error");
                }
            }
        }
        else
        {
            auto listener = reinterpret_cast<EventListener*>(ptr);
            queue_event(*listener);
        }
    }

    if(m_has_time_events)
    {
        //FIXME use a separate mutex for time events
        std::unique_lock event_listeners_lock(m_event_listeners_mutex);

        auto current_time = get_time();
        auto it = m_time_events.begin();
        
        while(it != m_time_events.end())
        {
            auto start = it->first;

            if(start <= current_time)
            {
                auto hdl = it->second;
                m_queued_events.push(hdl);
                m_num_queued_events++;

                auto res = eventfd_write(m_event_semaphore, 1);

                if(res < 0)
                {
                    throw std::runtime_error("Eventfd error");
                }

                it = m_time_events.erase(it);

                if(m_time_events.empty())
                {
                    m_has_time_events = false;
                }
            }
            else
            {
                break;
            }
        }
    }

    return update();
}

void EventLoop::queue_event(EventListener &l)
{
    std::shared_lock lock(m_event_listeners_mutex);

    auto idx = reinterpret_cast<uintptr_t>(&l);
    auto it = m_event_listeners.find(idx);

    if(it == m_event_listeners.end())
    {
        LOG(FATAL) << "Invalid event listener";
    }

    auto hdl = it->second;

    if(!hdl->active)
    {
        LOG(FATAL) << "Invalid state!";
    }

    hdl->num_events++;

    m_queued_events.push(hdl);
    m_num_queued_events++;
    
    auto res = eventfd_write(m_event_semaphore, 1);

    if(res < 0)
    {
        throw std::runtime_error("Eventfd error");
    }
}

void EventLoop::register_socket(int32_t fileno, void *ptr, int32_t flags)
{
    if(fileno <= 0)
    {
        throw std::runtime_error("Not a valid socket");
    }

    struct epoll_event ev;
    ev.events = flags < 0 ? EPOLL_FLAGS : flags;
    ev.data.ptr = ptr;

    int res = epoll_ctl(m_epoll_fd, EPOLL_CTL_ADD, fileno, &ev);
    if(res != 0)
    {
        LOG(FATAL) << "epoll_ctl() failed: " << strerror(errno);
    }
}

void EventLoop::unregister_socket(int32_t fileno)
{
    if(fileno <= 0)
    {
        throw std::runtime_error("Not a valid socket");
    }

    int res = epoll_ctl(m_epoll_fd, EPOLL_CTL_DEL, fileno, nullptr);
    (void)res; // ignore
}

void EventLoop::register_event_listener(EventListenerPtr listener)
{
    std::unique_lock lock(m_event_listeners_mutex);

    auto idx = reinterpret_cast<uintptr_t>(listener.get());
    auto hdl = new EventListenerHandle{.index = idx, .active = true, .num_events = 0, .pointer = listener};

    auto res = m_event_listeners.emplace(idx, hdl);
    if(!res.second)
    {
        LOG(FATAL) << "Listener already registered";
    }
}

void EventLoop::unregister_event_listener(EventListenerPtr listener)
{
    std::unique_lock lock(m_event_listeners_mutex);

    auto idx = reinterpret_cast<uintptr_t>(listener.get());
    auto it = m_event_listeners.find(idx);

    if(it == m_event_listeners.end())
    {
        LOG(FATAL) << "Event listener already unregistered";
    }
    else
    {
        auto hdl = it->second;

        if(!hdl->active)
        {
            LOG(FATAL) << "Event listener already unregistered";
        }

        hdl->active = false;

        if(hdl->num_events == 0)
        {
            delete hdl;
            m_event_listeners.erase(it);
        }
    }
}


void EventLoop::thread_loop()
{
    while(this->is_okay())
    {
        EventListenerHandle *hdl = update();

        if(hdl == nullptr)
        {
            // terminate
            return;
        }

        auto listener = hdl->pointer;
        listener->lock();
        listener->update();
        listener->unlock();

        auto last_val = hdl->num_events.fetch_sub(1);

        if(last_val < 1)
        {
            LOG(FATAL) << "Invalid state";
        }

        if(!hdl->active && last_val == 1)
        {
            std::unique_lock lock_guard(m_event_listeners_mutex);
            listener->unlock();

            auto it = m_event_listeners.find(hdl->index);

            if(it != m_event_listeners.end())
            {
                m_event_listeners.erase(it);
                delete hdl;
            }
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
