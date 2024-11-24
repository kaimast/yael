#include "yael/EventLoop.h"

#include <glog/logging.h>
#include <sys/epoll.h>
#include <sys/eventfd.h>
#include <unistd.h>

#include <cassert>
#include <chrono>

#include "yael/EventListener.h"

namespace yael {

/// A thread can handle at most one event
constexpr int32_t MAX_EVENTS = 1;

const uint32_t BASE_EPOLL_FLAGS = EPOLLERR | EPOLLRDHUP | EPOLLONESHOT;

// this code assumes epoll is thread-safe
// see http://lkml.iu.edu/hypermail/linux/kernel/0602.3/1661.html

inline uint32_t get_flags(EventListener::Mode mode) {
    if (mode == EventListener::Mode::ReadOnly) {
        return EPOLLIN | BASE_EPOLL_FLAGS;
    } else {
        return EPOLLIN | EPOLLOUT | BASE_EPOLL_FLAGS;
    }
}

inline void increment_semaphore(int32_t fd) {
    constexpr uint64_t SEMAPHORE_INC = 1;

    auto res = write(fd, &SEMAPHORE_INC, sizeof(SEMAPHORE_INC));
    if (res != sizeof(SEMAPHORE_INC)) {
        LOG(FATAL) << "eventfd write failed";
    }
}

inline void decrement_semaphore(int32_t fd) {
    uint64_t val = 0;

    auto res = read(fd, &val, sizeof(val));

    if (res != sizeof(val) || val == 0) {
        LOG(FATAL) << "Invalid state";
    }
}

EventLoop::EventLoop(int32_t num_threads)
    : m_okay(true), m_epoll_fd(epoll_create1(0)),
      m_event_semaphore(eventfd(0, EFD_NONBLOCK | EFD_SEMAPHORE)),
      m_num_threads(num_threads) {
    if (m_epoll_fd < 0) {
        LOG(FATAL) << "epoll_create1() failed: " << strerror(errno);
    }

    // TODO add a special semaphore event listener
    register_socket(m_event_semaphore, EPOLLIN | EPOLLET, false);
}

EventLoop::~EventLoop() { ::close(m_epoll_fd); }

EventLoop *EventLoop::m_instance = nullptr;

void EventLoop::initialize(int32_t num_threads) noexcept {
    if (m_instance != nullptr) {
        VLOG(1) << "Event loop already initialized. Will not do anything.";
        return;
    }

    m_instance = new EventLoop(num_threads);
    m_instance->run();
}

void EventLoop::destroy() noexcept {
    if (m_instance == nullptr) {
        LOG(FATAL) << "Cannot destroy event loop: instance does not exist";
    }

    if (m_instance->m_okay) {
        LOG(FATAL) << "Cannot stop event loop: has to be stopped first!";
    }

    delete m_instance;
    m_instance = nullptr;
}

void EventLoop::stop() noexcept {
    if (!m_okay) {
        VLOG(1)
            << "Already shutting down (or shut down). Will not stop event loop "
               "again.";
        return;
    }

    LOG(INFO) << "Shutting down event loop";

    std::unique_lock lock(m_event_listeners_mutex);
    m_okay = false;

    while (!m_event_listeners.empty()) {
        auto it = m_event_listeners.begin();
        auto listener = it->second;

        VLOG(2) << "Stopping next event listener (fileno="
                << listener->get_fileno() << ")";

        lock.unlock();
        listener->close_socket();
        lock.lock();
    }

    while (!m_event_listeners.empty()) {
        m_event_listeners_cond.wait(lock);
    }

    increment_semaphore(m_event_semaphore);
}

uint64_t EventLoop::get_time() const {
    using std::chrono::steady_clock;

    auto res = steady_clock::now();
    return std::chrono::duration_cast<std::chrono::milliseconds>(
               res.time_since_epoch())
        .count();
}

std::pair<EventListenerPtr, EventLoop::EventType> EventLoop::update() {
    epoll_event events[MAX_EVENTS];
    int nfds = -1;
    const int32_t timeout = -1;

    while (m_okay && nfds < 0) {
        nfds = epoll_wait(m_epoll_fd, events, MAX_EVENTS, timeout);
    }

    if (!m_okay && nfds <= 0) {
        // Event loop was terminated; wakeup next thread
        increment_semaphore(m_event_semaphore);
        return {nullptr, EventType::None};
    }

    if (nfds < 0) {
        // was interrupted by a signal. ignore
        // badf means the content server is shutting down
        if (errno == EINTR || errno == EBADF) {
            stop();
            return {nullptr, EventType::None};
        }

        // Let's try to continue here, if possible
        LOG(ERROR) << "epoll_wait() returned an error: " << strerror(errno)
                   << " (errno=" << errno << ")";
        return {nullptr, EventType::None};
    }

    if (nfds > 1) {
        LOG(FATAL) << "Invalid state: got more than one event";
    }

    auto fd = events[0].data.fd;

    if (fd == m_event_semaphore) {
        // Consume it so the event fd doesn't overflow
        decrement_semaphore(m_event_semaphore);

        if (m_okay) {
            DLOG(WARNING) << "Spurious wakeup";
            return update();
        } else {
            // Event loop was terminated; wake up next thread
            increment_semaphore(m_event_semaphore);
            return {nullptr, EventType::None};
        }
    } else {
        EventType type;
        auto flags = events[0].events;

        const bool has_read = (flags & EPOLLIN) != 0U;
        const bool has_write = (flags & EPOLLOUT) != 0U;
        const bool has_error = (flags & EPOLLERR) != 0U;

        if (has_read && has_write) {
            type = EventType::ReadWrite;
        } else if (has_read) {
            type = EventType::Read;
        } else if (has_write) {
            type = EventType::Write;
        } else if (has_error) {
            type = EventType::Error;
        } else {
            LOG(FATAL) << "Invalid event flag";
        }

        const std::shared_lock lock(m_event_listeners_mutex);
        auto it = m_event_listeners.find(fd);

        if (it == m_event_listeners.end()) {
            LOG(WARNING) << "Got event for unknown event listener with fileno="
                         << fd;
            return {nullptr, type};
        } else {
            return {it->second, type};
        }
    }
}

void EventLoop::register_event_listener(EventListenerPtr listener) noexcept {
    std::unique_lock lock(m_event_listeners_mutex);
    auto idx = listener->get_fileno();

    while (true) {
        auto res = m_event_listeners.emplace(idx, listener);

        if (res.second) {
            break;
        } else {
            // wait for other thread to process old event listener disconnect
            m_event_listeners_cond.wait(lock);
        }
    }

    lock.unlock();

    listener->re_register(true);
}

void EventLoop::register_socket(int32_t fileno, uint32_t flags, bool modify) {
    VLOG(2) << "Registering new socket with fd=" << fileno;

    struct epoll_event ev;
    ev.events = flags;
    ev.data.fd = fileno;

    auto op = modify ? EPOLL_CTL_MOD : EPOLL_CTL_ADD;

    const int epoll_res = epoll_ctl(m_epoll_fd, op, fileno, &ev);
    if (epoll_res != 0) {
        LOG(ERROR) << "epoll_ctl() failed: " << strerror(errno)
                   << " (fileno=" << fileno << ", modify=" << modify << ")";
    }
}

void EventLoop::notify_listener_mode_change(EventListenerPtr listener,
                                            EventListener::Mode mode,
                                            bool first_time) noexcept {
    VLOG(3) << "Event listener (fileno=" << listener->get_fileno()
            << ") mode changed to " << EventListener::mode_to_string(mode);

    auto flags = get_flags(mode);

    {
        const std::unique_lock lock(m_event_listeners_mutex);
        auto it = m_event_listeners.find(listener->get_fileno());

        if (it == m_event_listeners.end()) {
            // can happen during shut down
            LOG(WARNING) << "Failed to update mode for listener (fileno="
                         << listener->get_fileno()
                         << "): no such event listener";
            return;
        }
    }

    register_socket(listener->get_fileno(), flags, !first_time);
}

void EventLoop::unregister_event_listener(EventListenerPtr listener) noexcept {
    VLOG(2) << "Removing event listener (fileno=" << listener->get_fileno()
            << ")";

    const std::unique_lock lock(m_event_listeners_mutex);

    auto fileno = listener->get_fileno();
    auto it = m_event_listeners.find(fileno);

    if (it == m_event_listeners.end()) {
        LOG(WARNING) << "Could not unregister event listener. Did not exist?";
    } else {
        // (except for when releasing the socket manually)
        const auto epoll_res =
            epoll_ctl(m_epoll_fd, EPOLL_CTL_DEL, fileno, nullptr);
        if (epoll_res != 0) {
            LOG(ERROR) << "epoll_ctl() failed: " << strerror(errno)
                       << " (fileno=" << fileno << ")";
        }

        m_event_listeners.erase(it);
        m_event_listeners_cond.notify_all();
    }
}

void EventLoop::thread_loop() {
    while (this->is_okay()) {
        auto res = update();
        auto &[listener, type] = res;

        if (listener == nullptr) {
            // terminate
            return;
        }

        if (type == EventType::ReadWrite) {
            VLOG(3) << "Got read/write event";
            listener->on_read_ready();
            listener->on_write_ready();
        } else if (type == EventType::Read) {
            VLOG(3) << "Got read event";

            listener->on_read_ready();
        } else if (type == EventType::Write) {
            VLOG(3) << "Got write event";

            listener->on_write_ready();
        } else if (type == EventType::Error) {
            VLOG(3) << "Got error event";
            listener->on_error();
        } else {
            LOG(FATAL) << "Invalid event type!";
        }

        listener->re_register(false);
    }
}

void EventLoop::run() noexcept {
    auto num_threads = m_num_threads;

    if (num_threads <= 0) {
        num_threads =
            2 * static_cast<int32_t>(std::thread::hardware_concurrency());

        if (num_threads <= 0) {
            LOG(FATAL)
                << "Could not detect number of hardware threads supported!";
        }
    }

    for (auto i = 0; i < num_threads; ++i) {
        m_threads.emplace_back(&EventLoop::thread_loop, this);
    }

    LOG(INFO) << "Created new event loop with " << num_threads << " threads";
}

void EventLoop::wait() noexcept {
    for (auto &t : m_threads) {
        t.join();
    }
}

} // namespace yael
