#pragma once

#include "EventListener.h"

namespace yael
{

class SocketListener : public EventListener
{
public:
    virtual ~SocketListener() {}

    virtual bool is_valid() const = 0;
    virtual int32_t get_fileno() const = 0;
};

typedef std::shared_ptr<SocketListener> SocketListenerPtr;

}
