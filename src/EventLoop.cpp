#include "EventLoop.h"
#include "SocketListener.h"

#include <sys/epoll.h>

constexpr int32_t MAX_EVENTS = 1;
constexpr int32_t EPOLL_FLAGS = EPOLLIN | EPOLLERR | EPOLLRDHUP | EPOLLONESHOT;

using namespace yael;

EventLoop::EventLoop()
    : m_okay(true)
{
    m_epoll_fd = epoll_create1(0);
    if(m_epoll_fd < 0)
        LOG(FATAL) << "epoll_create1() failed: " << strerror(errno);
}

EventLoop::~EventLoop()
{
    ::close(m_epoll_fd);
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
