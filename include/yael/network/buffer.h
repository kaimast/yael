#pragma once

#include <cstdint>

namespace yael
{
namespace network
{

using msg_len_t = uint32_t;

struct buffer_t
{
    static constexpr int32_t MAX_SIZE = 4096;

    uint8_t data[MAX_SIZE];
    int32_t position;
    uint32_t size;

    buffer_t()
    {
        reset();
    }

    void reset()
    {
        size = 0;
        position = -1;
    }

    bool is_valid() const
    {
        return size > 0;
    }

    bool at_end() const
    {
        return position >= static_cast<int32_t>(size);
    }
};

}
}
