#include <gtest/gtest.h>
#include <glog/logging.h>

int main(int argc, char **argv)
{
    FLAGS_logbufsecs = 0;
    FLAGS_logbuflevel = google::GLOG_INFO;
    FLAGS_alsologtostderr = true;

    google::SetLogDestination(google::GLOG_INFO, "yael-test");
    google::InitGoogleLogging(argv[0]);

    testing::InitGoogleTest(&argc, argv);

    auto res = RUN_ALL_TESTS();

    google::ShutdownGoogleLogging();
    google::ShutDownCommandLineFlags();
    return res;
}
