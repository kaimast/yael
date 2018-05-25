#include "yael/TimeEventListener.h"

#include <sys/timerfd.h>

namespace yael
{

TimeEventListener::TimeEventListener()
{
    int32_t flags = 0;
    m_fileno = m_fd = timerfd_create(CLOCK_REALTIME, flags);
}

TimeEventListener::~TimeEventListener()
{
    if(m_fd != 0)
    {
        close(m_fd);
        m_fd = 0;
    }
}

void TimeEventListener::update()
{
    uint64_t buf;
    ::read(m_fd, &buf, sizeof(buf));

    if(buf == 1)
    {
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
    int32_t flags = 0;
    itimerspec new_value;
    new_value.it_interval.tv_sec = 0;
    new_value.it_interval.tv_nsec = 0;
    new_value.it_value.tv_sec = delay / 1000;
    new_value.it_value.tv_nsec = (delay % 1000) * (1000*1000);

    itimerspec old_value;

    auto res = timerfd_settime(m_fd, flags, &new_value, &old_value);

    if(res != 0)
    {
        LOG(ERROR) << "Failed to set time event";
    }
}

}
