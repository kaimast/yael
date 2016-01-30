#ifndef ADDRESSTEST_H
#define ADDRESSTEST_H

#include <gtest/gtest.h>
#include "network/Socket.h"

namespace yael
{
namespace network
{

class SocketTest : public testing::Test
{
protected:
    void resolveUrl_fromIP();
    void resolveUrl_localhost();
    void resolveUrl_cornell();
};

}
}

#endif // ADDRESSTEST_H

