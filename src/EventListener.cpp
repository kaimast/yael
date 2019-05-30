#include "yael/EventListener.h"
#include "yael/EventLoop.h"

namespace yael
{

void EventListener::set_mode(EventListener::Mode mode)
{
    auto old_val = m_mode.exchange(mode);

    if(old_val != mode)
    {
        auto &el = EventLoop::get_instance();
        el.notify_listener_mode_change(shared_from_this());
    }
}

}
