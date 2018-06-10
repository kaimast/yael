#pragma once

#include "EventListener.h"

namespace yael
{

class TimeEventListener : public EventListener
{
public:
    TimeEventListener();
    ~TimeEventListener();

    void schedule(uint64_t delay);

    virtual void on_time_event() = 0;

    /// Close the underlying socket
    void close();

private:
    int32_t get_fileno() const override final
    {
        return m_fileno;
    }

    bool is_valid() const override final
    {
        return m_fd > 0;
    }

    void update() override final;

    int32_t m_fileno;
    int32_t m_fd;
    
    bool m_is_scheduled = false;
};

}
