#include <iostream>
#include "memory.h"

namespace GameBoy {

std::uint8_t Memory::read_8(std::uint16_t address) const {
    return placeholder_memory[address];
}

void Memory::write_8(std::uint16_t address, std::uint8_t value) {
    placeholder_memory[address] = value;
}

std::uint16_t Memory::read_16(std::uint16_t address) const {
    // Little-endian read, first byte is least significant
    if (address == 0xFFFF) {
        std::cerr << "Warning: Attempted 16-bit read at 0xFFFF. Returning only the lower byte.\n";
        return placeholder_memory[address];
    }
    return placeholder_memory[address] | (placeholder_memory[address + 1] << 8);
}

void Memory::write_16(std::uint16_t address, std::uint16_t value) {
    // Little-endian write, first byte is least significant
    placeholder_memory[address] = value & 0xFF;
    if (address != 0xFFFF) {
        placeholder_memory[address + 1] = value >> 8;
    }
    else {
        std::cerr << "Warning: Attempted 16-bit write at 0xFFFF. Wrote only the lower byte.\n";
    }
}

} //namespace GameBoy
