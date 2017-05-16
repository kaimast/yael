#include "EventLoop.h"
#include "SocketListener.h"

#include <glog/logging.h>
#include <assert.h>
#include <chrono>
#include <sys/epoll.h>

// How many events can one thread see at most
// Increasing this value might make the scheduler fairer but also slower
constexpr int32_t MAX_EVENTS = 1;
constexpr int32_t TIMEOUT_MAX = 100;
constexpr int32_t EPOLL_FLAGS = EPOLLIN | EPOLLERR | EPOLLRDHUP | EPOLLONESHOT;

using namespace yael;

EventLoop::EventLoop(int32_t num_threads)
    : m_okay(true), m_epoll_fd(epoll_create1(0)), m_num_threads(num_threads), m_has_time_events(false)
{
    if(m_epoll_fd < 0)
        LOG(FATAL) << "epoll_create1() failed: " << strerror(errno);
}

EventLoop::~EventLoop()
{
    ::close(m_epoll_fd);
}

EventLoop* EventLoop::m_instance = nullptr;

void EventLoop::initialize(int32_t num_threads) throw(std::runtime_error)
{
    if(m_instance)
        throw std::runtime_error("EventLoop already initialized!");

    m_instance = new EventLoop(num_threads);
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

void EventLoop::register_time_event(uint64_t timeout, EventListener &listener)
{
    std::lock_guard<std::mutex> lock_guard(m_event_listeners_mutex);

    m_has_time_events = true;
    auto start = get_time() + timeout;

    for(auto it = m_time_events.begin(); it != m_time_events.end(); ++it)
    {
        if(it->first >= start)
        {
            m_time_events.emplace(it, std::pair<uint64_t, EventListener*>{start, &listener});
            return;
        }
    }

    m_time_events.emplace_back(std::pair<uint64_t, EventListener*>{start, &listener});
}

void EventLoop::update()
{
    EventListener *listener = get_next_event();

    if(!listener)
        return;

    listener->update();

    auto slistener = dynamic_cast<SocketListener*>(listener);

    if(slistener)
    {
        if(!slistener->is_valid())
        {
            m_event_listeners_mutex.lock();
            auto it = m_socket_listeners.find(slistener->get_fileno());
            m_socket_listeners.erase(it);
            m_event_listeners_mutex.unlock();

            delete listener;
        }
        else
        {
            //IMPORTANT unlock before you register with epoll. otherwise events could be discarded because of ONESHOT
            slistener->unlock();
            register_socket(slistener->get_fileno());
        }
    }
    else
    {
        // time event
        listener->unlock();
    }
}

EventListener* EventLoop::get_next_event()
{
    std::unique_lock<std::mutex> event_lock(m_queued_events_mutex);

    if(m_queued_events.size() == 0)
        pull_more_events();

    if(m_queued_events.size() > 0)
    {
        auto it = m_queued_events.begin();
        auto listener = *it;
        m_queued_events.erase(it);

        return listener;
    }
    else
        return nullptr;
}

uint64_t EventLoop::get_time() const
{
    using std::chrono::steady_clock;

    auto res = steady_clock::now();
    return std::chrono::duration_cast<std::chrono::milliseconds>(res.time_since_epoch()).count();
}

void EventLoop::pull_more_events()
{
    epoll_event events[MAX_EVENTS];

    std::unique_lock<std::mutex> epoll_lock(m_epoll_mutex);
    m_event_listeners_mutex.lock();
    int32_t timeout = TIMEOUT_MAX;
    if(m_has_time_events)
    {
        auto it = m_time_events.begin();
        const auto current_time = get_time();
        if(current_time >= it->first)
            timeout = 0;
        else
            timeout = static_cast<int32_t>(it->first - current_time);
    }
    m_event_listeners_mutex.unlock();

    int nfds = -1;
    while(nfds < 0 || (nfds == 0 && !m_has_time_events && m_okay))
        nfds = epoll_wait(m_epoll_fd, events, MAX_EVENTS, timeout);

    if(!m_okay) // Event loop was terminated
        return;

    if(nfds < 0)
    {
        // was interrupted by a signal. ignore
        // badf means the content server is shutting down
        if(errno == EINTR || errno == EBADF)
            return;

        LOG(FATAL) << "epoll_wait() returned an error: " << strerror(errno);
    }

    DLOG(INFO) << "Got " << nfds << " new events from epoll";

    m_event_listeners_mutex.lock();
    epoll_lock.unlock();

    for(int32_t i = 0; i < nfds; ++i)
    {
        auto fd = events[i].data.fd;
        auto it = m_socket_listeners.find(fd);

        if(it == m_socket_listeners.end())
        {
            DLOG(INFO) << "Discarded event as socket listner doesn't exist";
            continue; // might just have been closed...
        }

        auto listener = it->second;

        // Ignore if other thread is already handling it.
        if(listener->try_lock())
        {
            // Assumin m_queued_events is already locked
            m_queued_events.push_back(listener);
        }
        else
        {
            DLOG(INFO) << "Discarded event as object was locked";
            register_socket(listener->get_fileno());
        }
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
                e->lock();
                m_queued_events.push_back(e);
                it = m_time_events.erase(it);

                if(m_time_events.size() == 0)
                    m_has_time_events = false;
            }
            else
                break;
        }
    }

    m_event_listeners_mutex.unlock();
}

void EventLoop::register_socket(int32_t fileno)
{
    // Register again with epoll so other threads can receive data from it
    struct epoll_event ev;
    ev.events = EPOLL_FLAGS;
    ev.data.fd = fileno;

    int res = epoll_ctl(m_epoll_fd, EPOLL_CTL_MOD, fileno, &ev);
    if(res != 0 && errno != EBADF) // BADF might mean that the listener was just closed
        LOG(FATAL) << "epoll_ctl() failed: " << strerror(errno);
}

void EventLoop::register_socket_listener(int32_t fileno, SocketListener* listener)
{
    std::lock_guard<std::mutex> lock_guard(m_event_listeners_mutex);

    struct epoll_event ev;
    ev.events = EPOLL_FLAGS;
    ev.data.fd = fileno;

    int res = epoll_ctl(m_epoll_fd, EPOLL_CTL_ADD, fileno, &ev);
    if(res != 0)
        LOG(FATAL) << "epoll_ctl() failed: " << strerror(errno);

    m_socket_listeners[fileno] = listener;
}

void EventLoop::thread_loop()
{
    while(this->is_okay())
    {
        this->update();
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
