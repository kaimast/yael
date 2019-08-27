#pragma once

#include <chrono>
#include <mutex>

#include "EventListener.h"

namespace yael
{

class TimeEventListener : public EventListener
{
public:
    TimeEventListener();
    ~TimeEventListener();

    /// Trigger time event in [delay] ms from now
    bool schedule(uint64_t delay);

    /// Remove all time events for this object
    bool unschedule();

    virtual void on_time_event() = 0;

    /// Close the underlying socket
    virtual void close_socket() override;

    /// Get the current time (since unix epoch) in milliseconds
    uint64_t get_current_time() const
    {
        auto current_time = std::chrono::system_clock::now().time_since_epoch();
        return std::chrono::duration_cast<std::chrono::milliseconds>(current_time).count();
    }

    bool is_valid() override final
    {
        std::unique_lock lock(m_mutex);
        return m_fd >= 0;
    }

    EventListener::Mode mode() override
    {
        std::unique_lock lock(m_mutex);
        return m_mode;
    }

private:
    bool internal_schedule(uint64_t delay);

    void set_mode(EventListener::Mode mode);

    int32_t get_fileno() const override final
    {
        return m_fileno;
    }

    void on_read_ready() override final;
    void on_write_ready() override final {}
    void on_error() override final;

    int32_t m_fileno;
    int32_t m_fd;

    std::mutex m_mutex;
    std::vector<uint64_t> m_queued_events;

    EventListener::Mode m_mode;
};

}
