#pragma once

#include <gtest/gtest.h>
#include "yael/network/Socket.h"

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

