#pragma once

#include "yael/network/MessageSlicer.h"
#include <cmath>
#include <cstring>

namespace yael
{
namespace network
{

/**
 * Takes a stream of bytes and turns it into a sequence of messages
 *
 * Expected format: <size: uint32_t> <bytes: uint8_t[]>
 * where the length of bytes equals the value of size.
 */
class DatagramMessageSlicer : public MessageSlicer
{
public:
    static constexpr msg_len_t HEADER_SIZE = sizeof(msg_len_t);

    DatagramMessageSlicer() = default;

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
        return MessageMode::Datagram;
    }

    void prepare_message(std::unique_ptr<uint8_t[]> &ptr, uint32_t &length) override
    {
        auto cptr = ptr.get();
        ptr.release();

        auto payload_length = length;
        length = length + sizeof(length);

        cptr = reinterpret_cast<uint8_t*>(realloc(cptr, length));
        memmove(cptr+sizeof(length), cptr, payload_length);
        memcpy(cptr, reinterpret_cast<uint8_t*>(&length), sizeof(length));

        ptr = std::unique_ptr<uint8_t[]>(cptr);
    }

    bool get_message(message_in_t& message) override
    {
        if(!has_messages())
        {
            return false;
        }

        auto& it = m_messages.front();
        message.data = it.data;
        message.length = it.length - HEADER_SIZE;

        m_messages.pop_front();
        return true;
    }

    void process_buffer() override;

    /// Similar to Socket::message_in_t but also holds the message header and a read position
    struct message_in_t
    {
        message_in_t()
            : length(0), read_pos(0), data(nullptr)
        {
        }

        message_in_t(message_in_t &&other)
            : length(other.length), read_pos(other.read_pos), data(other.data)
        {
            other.length = other.read_pos = 0;
            other.data = nullptr;
        }

        void operator=(message_in_t &&other)
        {
            length = other.length;
            read_pos = other.read_pos;
            data = other.data;

            other.length = other.read_pos = 0;
            other.data = nullptr;
        }

        bool valid() const
        {
            return data != nullptr;
        }

        msg_len_t length;
        msg_len_t read_pos;
        uint8_t  *data;
    };

private:
    //! Stack of incoming messages
    //! used by pull_messages() and get_message()
    std::list<message_in_t> m_messages;

    //! Internal message buffer
    buffer_t m_buffer;

    //! Current position in the message buffer
    //! This is used for multiple messages in one receive call
    //! Message in progress to be read
    bool m_has_current_message = false;
    message_in_t m_current_message;
};

inline void DatagramMessageSlicer::process_buffer()
{
    message_in_t msg;
    bool received_full_msg = false;

    if(m_has_current_message)
    {
        msg = std::move(m_current_message);
        m_has_current_message = false;
    }
    
    // We need to read the header of the next datagram
    if(msg.read_pos < HEADER_SIZE)
    {
        int32_t readlength = std::min<int32_t>(HEADER_SIZE - msg.read_pos, m_buffer.size - m_buffer.position);

        memcpy(reinterpret_cast<char*>(&msg.length)+msg.read_pos, &m_buffer.data[m_buffer.position], readlength);

        msg.read_pos += readlength;
        m_buffer.position += readlength;

        if(msg.read_pos == HEADER_SIZE)
        {
            if(msg.length <= HEADER_SIZE)
            {
                throw std::runtime_error("Not a valid message");
            }

            msg.data = new uint8_t[msg.length - HEADER_SIZE];
        }
    }

    // Has header?
    if(msg.read_pos >= HEADER_SIZE)
    {
        const int32_t readlength = std::min(msg.length - msg.read_pos, m_buffer.size - m_buffer.position);

        if(readlength > 0)
        {
            if(msg.data == nullptr)
            {
                throw socket_error("Invalid state: message buffer not allocated");
            }

            mempcpy(&msg.data[msg.read_pos - HEADER_SIZE], &m_buffer.data[m_buffer.position], readlength);

            msg.read_pos += readlength;
            m_buffer.position += readlength;
        }

        if(msg.read_pos > msg.length)
        {
            throw socket_error("Invalid message length");
        }

        if(msg.read_pos == msg.length)
        {
            m_messages.emplace_back(std::move(msg));
            received_full_msg = true;
        }
    }

    if(!received_full_msg)
    {
        m_current_message = std::move(msg);
        m_has_current_message = true;
    }

    if(m_buffer.at_end())
    {
        m_buffer.reset();
    }
}


}
}
