#include "EventLoopTest.h"

using namespace yael;

TEST(EventLoopTest, start_stop_simple)
{
    EventLoop::initialize();
    auto &loop = EventLoop::get_instance();

    EXPECT_TRUE(loop.is_okay());

    loop.stop();

    EXPECT_FALSE(loop.is_okay());

    EventLoop::destroy();
}

TEST(EventLoopTest, start_stop)
{
     EventLoop::initialize();
    auto &loop = EventLoop::get_instance();

    EXPECT_TRUE(loop.is_okay());

    loop.stop();

    loop.wait();

    EXPECT_FALSE(loop.is_okay());

    EventLoop::destroy();
}
