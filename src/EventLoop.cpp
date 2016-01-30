#include "EventLoop.h"
#include "SocketListener.h"

#include <thread>
#include <stack>
#include <sys/epoll.h>

constexpr int32_t MAX_EVENTS = 1;
constexpr int32_t EPOLL_FLAGS = EPOLLIN | EPOLLERR | EPOLLRDHUP | EPOLLONESHOT;

using namespace yael;

EventLoop::EventLoop(int32_t num_threads)
    : m_okay(true), m_epoll_fd(epoll_create1(0)), m_num_threads(num_threads)
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

void EventLoop::destroy() throw(std::runtime_error)
{
    if(!m_instance)
        throw std::runtime_error("EventLoop does not exist (anymore)");

    delete m_instance;
    m_instance = nullptr;
}

void EventLoop::stop()
{
    m_okay = false;
}

void EventLoop::update()
{
    epoll_event events[MAX_EVENTS];

    std::unique_lock<std::mutex> epoll_lock(m_epoll_mutex);

    int nfds = 0;
    while(nfds == 0 && m_okay)
        nfds = epoll_wait(m_epoll_fd, events, MAX_EVENTS, 1000);

    if(nfds == 0)
        return; // server shut down

    if(nfds < 0)
    {
        // was interrupted by a signal. ignore
        // badf means the content server is shutting down
        if(errno == EINTR || errno == EBADF)
            return;

        LOG(FATAL) << "epoll_wait() returned an error: " << strerror(errno);
    }

    DLOG(INFO) << "Got " << nfds << " new events from epoll";

    std::vector<SocketListener*> listeners;

    std::unique_lock<std::mutex> socket_listeners_lock(m_socket_listeners_mutex);
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
            listeners.push_back(listener);
        else
        {
            DLOG(INFO) << "Discarded event as object was locked";

            // Register again with epoll so other threads can receive data from it
            struct epoll_event ev;
            ev.events = EPOLL_FLAGS;
            ev.data.fd = fd;

            int res = epoll_ctl(m_epoll_fd, EPOLL_CTL_MOD, fd, &ev);
            if(res != 0 && errno != EBADF) // BADF might mean that the listener was just closed
                 LOG(FATAL) << "epoll_ctl() failed: " << strerror(errno);
        }
    }

    socket_listeners_lock.unlock();

    for(auto it = listeners.begin(); it != listeners.end(); ++it)
    {
        SocketListener* listener = (*it);
        listener->update();

        if(!listener->is_valid())
        {
            std::lock_guard<std::mutex> lock_guard(m_socket_listeners_mutex);

            auto it2 = m_socket_listeners.find(listener->get_fileno());
            m_socket_listeners.erase(it2);

            delete listener;
        }
        else
        {
            //IMPORTANT unlock before you register with epoll. otherwise events could be discarded because of ONESHOT
            listener->unlock();

            // Register again with epoll so other threads can receive data from it
            struct epoll_event ev;
            ev.events = EPOLL_FLAGS;
            ev.data.fd = listener->get_fileno();

            int res = epoll_ctl(m_epoll_fd, EPOLL_CTL_MOD, listener->get_fileno(), &ev);
            if(res != 0 && errno != EBADF) // BADF might mean that the listener was just closed
                 LOG(FATAL) << "epoll_ctl() failed: " << strerror(errno);
        }
    }
}

void EventLoop::register_socket_listener(int32_t fileno, SocketListener* listener)
{
    std::lock_guard<std::mutex> lock_guard(m_socket_listeners_mutex);

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
    std::stack<std::thread> threads;

    if(num_threads <= 0)
    {
        num_threads = 2 * std::thread::hardware_concurrency();

        if(num_threads <= 0)
            throw std::runtime_error("Could not detect number of hardware threads supported!");
    }

    for(int32_t i = 0; i < num_threads; ++i)
    {
        threads.push(std::thread(&EventLoop::thread_loop, this));
    }

    while(!threads.empty())
    {
        threads.top().join();
        threads.pop();
    }
}
