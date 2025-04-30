#pragma once

#include <iomanip>
#include <iostream>

#include "bitwise_utilities.h"
#include "central_processing_unit.h"
#include "memory_management_unit.h"

namespace GameBoy
{

inline void print_bytes_in_range(MemoryManagementUnit &memory_management_unit, uint16_t start_address, uint16_t end_address)
{
    std::cout << std::hex << std::setfill('0');
    std::cout << "=========== Memory Range 0x" << std::setw(4) << start_address << " - 0x" << std::setw(4) << end_address << " ============\n";

    for (uint32_t address = start_address; address <= end_address; address++)
    {
        uint16_t remainder = address % 0x10;

        if (address == start_address || remainder == 0)
        {
            uint16_t line_offset = address - remainder;
            std::cout << std::setw(4) << line_offset << ": ";

            for (uint16_t i = 0; i < remainder; i++)
                std::cout << "   ";
        }

        std::cout << std::setw(2) << static_cast<int>(memory_management_unit.read_byte(address, false)) << " ";

        if ((address + 1) % 0x10 == 0)
            std::cout << "\n";
    }

    if ((end_address + 1) % 0x10 != 0)
        std::cout << "\n";

    std::cout << "=====================================================\n";
}

inline void print_register_file_state(RegisterFile<std::endian::native> register_file)
{
    std::cout << "=================== CPU Registers ===================\n";
    std::cout << std::hex << std::setfill('0');

    std::cout << "AF: 0x" << std::setw(4) << register_file.af << "   "
              << "(A: 0x" << std::setw(2) << static_cast<int>(register_file.a) << ","
              << " F: 0x" << std::setw(2) << static_cast<int>(register_file.flags) << ")   "
              << "Flags ZNHC: " << (is_flag_set(register_file.flags, FLAG_ZERO_MASK) ? "1" : "0")
              << (is_flag_set(register_file.flags, FLAG_SUBTRACT_MASK) ? "1" : "0")
              << (is_flag_set(register_file.flags, FLAG_HALF_CARRY_MASK) ? "1" : "0")
              << (is_flag_set(register_file.flags, FLAG_CARRY_MASK) ? "1" : "0") << "\n";

    std::cout << "BC: 0x" << std::setw(4) << register_file.bc << "   "
              << "(B: 0x" << std::setw(2) << static_cast<int>(register_file.b) << ","
              << " C: 0x" << std::setw(2) << static_cast<int>(register_file.c) << ")\n";

    std::cout << "DE: 0x" << std::setw(4) << register_file.de << "   "
              << "(D: 0x" << std::setw(2) << static_cast<int>(register_file.d) << ","
              << " E: 0x" << std::setw(2) << static_cast<int>(register_file.e) << ")\n";

    std::cout << "HL: 0x" << std::setw(4) << register_file.hl << "   "
              << "(H: 0x" << std::setw(2) << static_cast<int>(register_file.h) << ","
              << " L: 0x" << std::setw(2) << static_cast<int>(register_file.l) << ")\n";

    std::cout << "Stack Pointer: 0x" << std::setw(4) << register_file.stack_pointer << "\n";
    std::cout << "Program Counter: 0x" << std::setw(4) << register_file.program_counter << "\n";
    std::cout << "=====================================================\n";
}

} // namespace GameBoy
