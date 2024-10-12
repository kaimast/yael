#pragma once

#include <memory>
#include <glog/logging.h>

namespace yael
{

class EventListener : public std::enable_shared_from_this<EventListener>
{
public:
    enum class Mode
    {
        ReadOnly,
        ReadWrite
    };

    virtual ~EventListener() = default;

    /**
     * Handle events
     */
    virtual void on_read_ready() = 0;
    virtual void on_write_ready() = 0;
    virtual void on_error() = 0;

    /**
     * Is the underlying socket still valid
     */
    virtual bool is_valid() = 0;

    /** 
     * What is the socket's filedescriptor?
     * (used to identify this listener
     */
    virtual int32_t get_fileno() const = 0;

    /// Event(s) have been handled. Re-register if desired
    virtual void re_register(bool first_time) = 0;

    virtual void close_socket() = 0;

    static const char* mode_to_string(Mode &mode) {
        switch(mode) {
            case Mode::ReadOnly: return "read-only";
            case Mode::ReadWrite: return "read-write";
            default: return "unknown";
        }
    }

protected:
    EventListener() = default;
};

using EventListenerPtr = std::shared_ptr<EventListener>;


}
