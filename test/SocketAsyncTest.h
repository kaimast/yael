#ifndef SOCKETASYNCTEST_H
#define SOCKETASYNCTEST_H

#include <gtest/gtest.h>
#include "network/Socket.h"

namespace yael
{
namespace network
{

class SocketAsyncTest : public testing::Test
{
protected:
    void send_lage_chunk();
};

}
}

#endif // SOCKETASYNCTEST_H

