#pragma once

#include "EventListener.h"

namespace yael
{

class SocketListener : public EventListener
{
public:
    virtual ~SocketListener() {}

    /**
     * Does the underlying socket have messages
     */
    virtual bool is_valid() const = 0;

    /** 
     * What is the socket's filedescriptor?
     * (used to identify this listener
     */
    virtual int32_t get_fileno() const = 0;

    virtual bool has_messages() const = 0;
};

typedef std::shared_ptr<SocketListener> SocketListenerPtr;

}
