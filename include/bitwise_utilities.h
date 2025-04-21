#pragma once

namespace GameBoy
{

template<typename T>
inline bool is_bit_set(T value, uint8_t bit_position_to_test)
{
    return (value & (static_cast<T>(1) << bit_position_to_test)) != 0;
}

template<typename T>
inline void set_bit(T &variable, uint8_t bit_position, bool new_bit_state)
{
    if (new_bit_state)
        variable |= (static_cast<T>(1) << bit_position);
    else
        variable &= ~(static_cast<T>(1) << bit_position);
}

template<typename T>
inline bool is_flag_set(T value, T flag_mask)
{
    return (value & flag_mask) != 0;
}

template<typename T>
inline void update_flag(T &variable, T flag_mask, bool new_flag_state)
{
    variable = new_flag_state
        ? (variable | flag_mask)
        : (variable & ~flag_mask);
}

}
