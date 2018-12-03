#pragma once

#include <chrono>
#include "EventListener.h"

namespace yael
{

class TimeEventListener : public EventListener
{
public:
    TimeEventListener();
    ~TimeEventListener();

    /// Trigger time event in [delay] ms from now
    void schedule(uint64_t delay);

    /// Remove all time events for this object
    void unschedule();

    virtual void on_time_event() = 0;

    /// Close the underlying socket
    virtual void close_socket() override;

    /// Get the current time (since unix epoch) in milliseconds
    uint64_t get_current_time() const
    {
        auto current_time = std::chrono::system_clock::now().time_since_epoch();
        return std::chrono::duration_cast<std::chrono::milliseconds>(current_time).count();
    }

    bool is_valid() const override final
    {
        return m_fd >= 0;
    }

private:
    void internal_schedule(uint64_t delay);

    int32_t get_fileno() const override final
    {
        return m_fileno;
    }

    void on_read_ready() override final;
    void on_write_ready() override final {}

    int32_t m_fileno;
    int32_t m_fd;

    std::vector<uint64_t> m_queued_events;
};

}
