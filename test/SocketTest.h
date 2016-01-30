#pragma once

#include <gtest/gtest.h>
#include "network/Socket.h"

namespace yael
{
namespace network
{

class SocketTest : public testing::Test
{
protected:
    void listens();
    void connected();

    void send_one_way();
    void send_other_way();

    void SetUp() override
    {
        Address addr = resolveUrl("localhost", 62123);
        bool listening = m_socket1.listen(addr.IP, addr.PortNumber, 10);
        ASSERT_TRUE(listening);

        bool connected = m_socket2.connect(addr);
        ASSERT_TRUE(connected);

        m_peer_socket = m_socket1.accept();
        assert(m_peer_socket);
    }

    void TearDown() override
    {
        delete m_peer_socket;
    }

    Socket m_socket1, m_socket2;
    Socket *m_peer_socket;
};

}
}
