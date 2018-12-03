
#include <sys/timerfd.h>
#include <yael/EventLoop.h>
#include <yael/TimeEventListener.h>

namespace yael
{

TimeEventListener::TimeEventListener()
    : EventListener(EventListener::Mode::ReadOnly)
{
    constexpr int32_t flags = 0;
    m_fileno = m_fd = timerfd_create(CLOCK_REALTIME, flags);
}

TimeEventListener::~TimeEventListener()
{
    close_socket();
}

void TimeEventListener::close_socket()
{
    if(!is_valid())
    {
        return;
    }

    ::close(m_fd);
    m_fd = -1;

    if(EventLoop::is_initialized())
    {
        auto &el = EventLoop::get_instance();
        el.unregister_event_listener(shared_from_this());
    }
}

void TimeEventListener::on_read_ready()
{
    uint64_t buf;
    ::read(m_fd, &buf, sizeof(buf));

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
                // erase befroe we invoke the callback
                // because application code might call schedule()
                m_queued_events.erase(it);
                this->on_time_event();

                count++;
            }
            else
            {
                break;
            }
        }

        if(!m_queued_events.empty() && is_valid())
        {
            auto next = m_queued_events[0];

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

void TimeEventListener::schedule(uint64_t delay)
{
    if(!is_valid())
    {
        throw std::runtime_error("Cannot schedule event: socket already closed");
    }

    bool is_scheduled = !m_queued_events.empty();
    bool inserted = false;
    bool first = true;

    auto start = get_current_time() + delay;

    for(auto it = m_queued_events.begin(); !inserted && it != m_queued_events.end(); ++it)
    {
        if(*it > start)
        {
            m_queued_events.insert(it, start);
            inserted = true;
        }
        else
        {
            first = false;
        }
    }

    if(!inserted)
    {
        m_queued_events.push_back(start);
    }

    if(first)
    {
        is_scheduled = false;
    }

    if(!is_scheduled)
    {
        internal_schedule(delay);
    }
}

void TimeEventListener::unschedule()
{
    m_queued_events.clear();
}

void TimeEventListener::internal_schedule(uint64_t delay)
{
    int32_t flags = 0;
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
        new_value.it_value.tv_sec = delay / 1000;
        new_value.it_value.tv_nsec = (delay % 1000) * (1000*1000);
    }

    itimerspec old_value;

    auto res = timerfd_settime(m_fd, flags, &new_value, &old_value);

    if(res != 0)
    {
        LOG(ERROR) << "Failed to set time event";
    }
}

}
