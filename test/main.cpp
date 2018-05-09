#include <gtest/gtest.h>
#include <glog/logging.h>

int main(int argc, char **argv)
{
    google::SetLogDestination(google::GLOG_INFO, "credb-test");
    google::InitGoogleLogging(argv[0]);

    FLAGS_logbufsecs = 0;
    FLAGS_logbuflevel = google::GLOG_INFO;
    FLAGS_alsologtostderr = 1;

    testing::InitGoogleTest(&argc, argv);

    auto res = RUN_ALL_TESTS();

    google::ShutdownGoogleLogging();
    google::ShutDownCommandLineFlags();
    return res;
}
