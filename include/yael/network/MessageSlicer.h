#pragma once

#include <memory>
#include "buffer.h"

namespace yael
{
namespace network
{

struct message_in_t
{
    uint8_t *data;
    msg_len_t length;
};

enum class MessageMode
{
    /*
     * Will ensure messages arrive as whole by encoding message length
     */
    Datagram,

    /*
     * Send data as-is, will arrive in order but might fragmented
     * Use this if you want to build your own protocol on top of low-level streams
     */
    Stream
};

class MessageSlicer
{
public:
    virtual ~MessageSlicer() = default;

    virtual MessageMode type() const = 0;

    virtual bool has_messages() const = 0;

    virtual void prepare_message(std::unique_ptr<uint8_t[]> &ptr, uint32_t &length) = 0;

    virtual buffer_t& buffer() = 0;

    virtual void process_buffer() = 0;

    virtual bool get_message(message_in_t& message) = 0;
};

}
}
