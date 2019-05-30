#pragma once

#include "yael/network/MessageSlicer.h"
#include <cmath>
#include <cstring>

namespace yael
{
namespace network
{

/**
 * This basically doesn't modify the data stream at all
 */
class StreamMessageSlicer : public MessageSlicer
{
public:
    StreamMessageSlicer() = default;

    buffer_t& buffer() override
    {
        return m_buffer;
    }
    
    bool has_messages() const override
    {
        return !m_messages.empty();
    }
    
    MessageMode type() const override
    {
        return MessageMode::Stream;
    }

    void prepare_message(std::unique_ptr<uint8_t[]> &ptr, uint32_t &length) override
    {
        // no-op
        (void)ptr;
        (void)length;
    }

    bool get_message(Socket::message_in_t& message) override
    {
        if(!has_messages())
        {
            return false;
        }

        auto& it = m_messages.front();
        message.data = it.data;
        message.length = it.length;

        m_messages.pop_front();
        return true;
    }

    void process_buffer() override;

private:
    //! Stack of incoming messages
    //! used by pull_messages() and get_message()
    std::list<Socket::message_in_t> m_messages;

    //! Internal message buffer
    buffer_t m_buffer;
};

inline void StreamMessageSlicer::process_buffer()
{
    Socket::message_in_t msg;

    // turn current buffer into message
    if(m_buffer.size <= 0)
    {
        return;
    }

    msg.length = m_buffer.size;
    msg.data = reinterpret_cast<uint8_t*>(malloc(msg.length));
    memcpy(msg.data, m_buffer.data, msg.length);

    m_messages.emplace_back(std::move(msg));
    m_buffer.reset();
}

}
}
