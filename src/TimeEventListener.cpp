#include <unistd.h>

#include <sys/timerfd.h>
#include <yael/EventLoop.h>
#include <yael/TimeEventListener.h>

namespace yael
{

TimeEventListener::TimeEventListener()
{
    constexpr int32_t flags = 0;
    m_fileno = m_fd = timerfd_create(CLOCK_REALTIME, flags);
}

TimeEventListener::~TimeEventListener() = default;

void TimeEventListener::close_socket()
{
    std::unique_lock lock(m_mutex);

    if(m_fd < 0)
    {
        return;
    }

    ::close(m_fd);
    m_fd = -1;

    lock.unlock();

    if(EventLoop::is_initialized())
    {
        auto &el = EventLoop::get_instance();
        el.unregister_event_listener(shared_from_this());
    }
}

void TimeEventListener::on_error()
{
    LOG(WARNING) << "Got error; closing socket";
    close_socket();
}

void TimeEventListener::re_register(bool first_time)
{
    if(!is_valid())
    {
        return;
    }

    auto &el = EventLoop::get_instance();
    el.notify_listener_mode_change(shared_from_this(), EventListener::Mode::ReadOnly, first_time);
}

void TimeEventListener::on_read_ready()
{
    std::unique_lock lock(m_mutex);

    uint64_t buf;
    auto res = ::read(m_fd, &buf, sizeof(buf));

    if(res != sizeof(buf))
    {
        LOG(FATAL) << "Failed to read from timefd";
    }

    if(buf == 1)
    {
        auto now = get_current_time();
        size_t count = 0;

        while(true)
        {
            if(m_queued_events.empty())
            {
                break;
            }

            auto it = m_queued_events.begin();
            if(*it <= now)
            {
                // erase before we invoke the callback
                // because application code might call schedule()
                m_queued_events.erase(it);
                count++;
            }
            else
            {
                break;
            }
        }

        VLOG(1) << "Found " << count << " time events to trigger";

        lock.unlock();
        for(size_t i = 0; i < count; ++i)
        {
            this->on_time_event();
        }
        lock.lock();

        if(!m_queued_events.empty() && m_fd >= 0)
        {
            auto next = *m_queued_events.begin();

            if(next < now)
            {
                LOG(FATAL) << "Invalid state";
            }

            internal_schedule(next - now);
        }
    }
    else if(buf == 0)
    {
        DLOG(WARNING) << "Spurious wakeup";
    }
    else
    {
        LOG(ERROR) << "Got more than one timeout!";
    }
}

bool TimeEventListener::schedule(uint64_t delay)
{
    const std::unique_lock lock(m_mutex);

    if(m_fd < 0)
    {
        LOG(WARNING) << "Cannot schedule event: socket already closed";
        return false;
    }

    bool is_scheduled = !m_queued_events.empty();
    
    auto start = get_current_time() + delay;
    auto it = m_queued_events.insert(start);

    if(it == m_queued_events.begin())
    {
        is_scheduled = false;
    }

    if(is_scheduled)
    {
        return true;
    }

    return internal_schedule(delay);
}

bool TimeEventListener::unschedule()
{
    const std::unique_lock lock(m_mutex);

    const bool has_events = !m_queued_events.empty();
    m_queued_events.clear();

    return has_events;
}

bool TimeEventListener::internal_schedule(uint64_t delay)
{
    const auto flags = 0;
    itimerspec new_value;
    new_value.it_interval.tv_sec = 0;
    new_value.it_interval.tv_nsec = 0;

    if(delay == 0)
    {
        // Setting the delay to 0 disarms the timer
        // Instead we set it to 1ns as a workaround
        new_value.it_value.tv_sec = 0;
        new_value.it_value.tv_nsec = 1;
    }
    else
    {
        new_value.it_value.tv_sec = static_cast<__syscall_slong_t>(delay) / 1000;
        new_value.it_value.tv_nsec = static_cast<__syscall_slong_t>(delay % 1000) * 1'000'000;
    }

    itimerspec old_value;

    auto res = timerfd_settime(m_fd, flags, &new_value, &old_value);

    if(res != 0)
    {
        LOG(ERROR) << "Failed to set time event";
        return false;
    }

    return true;
}

}
