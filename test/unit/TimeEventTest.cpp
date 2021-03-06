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

class TestTimeListener2 : public TimeEventListener
{
public:
    void on_time_event() override
    {
        count += 1;
        if(count < 10)
        {
            this->schedule(100);
        }
    }

    uint32_t count = 0;
};


TEST(TimeEventTest, multi_schedule)
{
    EventLoop::initialize();
    auto &el = EventLoop::get_instance();

    const uint32_t expected1 = 1;
    const uint32_t expected2 = 2;

    auto hdl = el.make_event_listener<TestTimeListener>();
    hdl->schedule(100);

    while(hdl->count < expected1)
    {
        //pass
    }

    EXPECT_EQ(expected1, hdl->count);

    hdl->schedule(100);

    while(hdl->count < expected2)
    {
        //pass
    }

    EXPECT_EQ(expected2, hdl->count);

    el.stop();
    el.wait();

    EventLoop::destroy();
}

TEST(TimeEventTest, self_schedule)
{
    EventLoop::initialize();
    auto &el = EventLoop::get_instance();

    constexpr uint64_t expected = 10;
    auto hdl = el.make_event_listener<TestTimeListener2>();

    hdl->schedule(0);

    while(hdl->count != expected)
    {
        //pass
    }

    el.stop();
    el.wait();

    EventLoop::destroy();

    EXPECT_EQ(expected, hdl->count);
}

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

    EXPECT_EQ(3U, hdl->count);
}

TEST(TimeEventTest, schedule_three2)
{
    EventLoop::initialize();
    auto &el = EventLoop::get_instance();

    auto hdl = el.make_event_listener<TestTimeListener>();

    hdl->schedule(0);
    hdl->schedule(0);
    hdl->schedule(0);

    while(hdl->count != 3)
    {
        //pass
    }

    el.stop();
    el.wait();

    EventLoop::destroy();

    EXPECT_EQ(3U, hdl->count);
}
