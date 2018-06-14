#include <gtest/gtest.h>
#include <yael/TimeEventListener.h>
#include <yael/EventLoop.h>

using namespace yael;

class TimeEventTest : public testing::Test
{
};

class TestTimeListener : public TimeEventListener
{
public:
    void on_time_event() override
    {
        count += 1;
    }

    uint32_t count = 0;
};

TEST(TimeEventTest, schedule_three)
{
    EventLoop::initialize();
    auto &el = EventLoop::get_instance();

    auto hdl = el.make_event_listener<TestTimeListener>();
    hdl->schedule(200);
    hdl->schedule(100);
    hdl->schedule(400);

    while(hdl->count != 3)
    {
        //pass
    }

    el.stop();
    el.wait();

    EventLoop::destroy();
}
