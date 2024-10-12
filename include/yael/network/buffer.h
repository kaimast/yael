#pragma once

#include <cstdint>

namespace yael::network {

using msg_len_t = uint32_t;

struct buffer_t
{
    static constexpr int32_t MAX_SIZE = 4096;
    
    buffer_t()
    {
        reset();
    }

    void reset()
    {
        m_size = 0;
        m_position = -1;
    }

    [[nodiscard]]
    bool is_valid() const 
    {
        return m_size > 0;
    }

    [[nodiscard]]
    bool at_end() const
    {
        return m_position >= static_cast<int32_t>(m_size);
    }

    uint32_t size() {
        return m_size;
    }

    int32_t position() {
        return m_position;
    }

    bool is_empty() {
        return m_size == 0;
    }

    uint8_t* data() {
        return m_data;
    }

    void advance_position(int32_t advanceby) {
        m_position += advanceby;
    }

    void set_position(int32_t newpos) {
        m_position = newpos;
    }

    void set_size(uint32_t newsize) {
        m_size = newsize;
    }

private:
    uint8_t m_data[MAX_SIZE];
    int32_t m_position;
    uint32_t m_size;
};

}
