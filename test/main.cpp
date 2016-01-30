//#include "src/sighandler.h"

#include <gtest/gtest.h>
#include <gmock/gmock.h>

int main(int argc, char **argv)
{
   // install_sighandler();

    ::testing::InitGoogleMock(&argc, argv);
    return RUN_ALL_TESTS();
}
