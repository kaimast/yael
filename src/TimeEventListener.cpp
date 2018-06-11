
#include <sys/timerfd.h>
#include <yael/EventLoop.h>
#include <yael/TimeEventListener.h>

namespace yael
{

TimeEventListener::TimeEventListener()
{
    int32_t flags = 0;
    m_fileno = m_fd = timerfd_create(CLOCK_REALTIME, flags);
}

TimeEventListener::~TimeEventListener()
{
    close();
}

void TimeEventListener::close()
{
    if(m_fd > 0 && !m_is_scheduled)
    {
        ::close(m_fd);
        m_fd = -1;

        if(EventLoop::is_initialized())
        {
            auto &el = EventLoop::get_instance();
            el.unregister_event_listener(shared_from_this());
        }
    }
}

void TimeEventListener::update()
{
    uint64_t buf;
    ::read(m_fd, &buf, sizeof(buf));

    if(buf == 1)
    {
        m_is_scheduled = false;
        this->on_time_event();
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

    if(m_is_scheduled)
    {
        throw std::runtime_error("Cannot schedule: Time event listener already scheduled");
    }

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

    if(res == 0)
    {
        m_is_scheduled = true;
    }
    else
    {
        LOG(ERROR) << "Failed to set time event";
    }
}

}
