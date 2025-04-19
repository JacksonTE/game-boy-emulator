#pragma once

#include <iomanip>
#include <iostream>
#include "memory_management_unit.h"

namespace GameBoy {

template<typename T>
inline bool is_bit_set(T value, uint8_t bit_position_to_test)
{
    return (value & (1 << bit_position_to_test)) != 0;
}

template<typename T>
inline void set_bit(T &variable, uint8_t bit_position, bool new_bit_state)
{
    if (new_bit_state)
        variable |= (1 << bit_position);
    else
        variable &= ~(1 << bit_position);
}

inline void print_bytes_in_range(MemoryManagementUnit &memory_management_unit, uint16_t start_address, uint16_t end_address)
{
    std::cout << std::hex << std::setfill('0');
    std::cout << "=========== Memory Range 0x" << std::setw(4) << start_address << " - 0x" << std::setw(4) << end_address << " ============\n";

    for (uint16_t address = start_address; address <= end_address; address++)
    {
        uint16_t remainder = address % 0x10;

        if (address == start_address || remainder == 0)
        {
            uint16_t line_offset = address - remainder;
            std::cout << std::setw(4) << line_offset << ": ";

            for (uint16_t i = 0; i < remainder; i++)
                std::cout << "   ";
        }

        std::cout << std::setw(2) << static_cast<int>(memory_management_unit.read_byte(address)) << " ";

        if ((address + 1) % 0x10 == 0)
            std::cout << "\n";
    }

    if ((end_address + 1) % 0x10 != 0)
        std::cout << "\n";

    std::cout << "=====================================================\n";
}

} // namespace GameBoy
