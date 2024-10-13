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

    int get_count() {
        return count;
    } 

private:
    volatile int count = 0;
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

    int get_count() {
        return count;
    } 

private:
    volatile int count = 0;
};


TEST(TimeEventTest, multi_schedule)
{
    EventLoop::initialize();
    auto &el = EventLoop::get_instance();

    const auto expected1 = 1;
    const auto expected2 = 2;

    auto hdl = el.make_event_listener<TestTimeListener>();
    hdl->schedule(100);

    while(hdl->get_count() < expected1)
    {
        //pass
    }

    VLOG(1) << "Got first time event";
    EXPECT_EQ(expected1, hdl->get_count());

    hdl->schedule(100);

    while(hdl->get_count() < expected2)
    {
        //pass
    }

    VLOG(1) << "Got second time event";
    EXPECT_EQ(expected2, hdl->get_count());

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

    while(hdl->get_count() != expected)
    {
        //pass
    }

    el.stop();
    el.wait();

    EventLoop::destroy();

    EXPECT_EQ(expected, hdl->get_count());
}

TEST(TimeEventTest, schedule_three)
{
    EventLoop::initialize();
    auto &el = EventLoop::get_instance();

    auto hdl = el.make_event_listener<TestTimeListener>();

    hdl->schedule(200);
    hdl->schedule(100);
    hdl->schedule(400);

    while(hdl->get_count() != 3)
    {
        //pass
    }

    el.stop();
    el.wait();

    EventLoop::destroy();

    EXPECT_EQ(3U, hdl->get_count());
}

TEST(TimeEventTest, schedule_three2)
{
    EventLoop::initialize();
    auto &el = EventLoop::get_instance();

    auto hdl = el.make_event_listener<TestTimeListener>();

    hdl->schedule(0);
    hdl->schedule(0);
    hdl->schedule(0);

    while(hdl->get_count() != 3)
    {
        //pass
    }

    el.stop();
    el.wait();

    EventLoop::destroy();

    EXPECT_EQ(3U, hdl->get_count());
}
