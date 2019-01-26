#pragma once

#include <memory>
#include "Socket.h"

namespace yael
{
namespace network
{

enum class MessageMode
{
    Datagram,
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

    virtual bool get_message(Socket::message_in_t& message) = 0;
};

}
}
