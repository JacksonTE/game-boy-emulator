#include <iostream>
#include "memory.h"

namespace GameBoy {

uint8_t Memory::read_8(uint16_t address) const {
    return placeholder_memory[address];
}

void Memory::write_8(uint16_t address, uint8_t value) {
    placeholder_memory[address] = value;
}

uint16_t Memory::read_16(uint16_t address) const {
    // Little-endian read, first byte is least significant
    if (address == 0xffff) {
        std::cerr << "Warning: Attempted 16-bit read at 0xffff. Returning only the lower byte.\n";
        return placeholder_memory[address];
    }
    return (placeholder_memory[address + 1] << 8) | placeholder_memory[address];
}

void Memory::write_16(uint16_t address, uint16_t value) {
    // Little-endian write, first byte is least significant
    placeholder_memory[address] = value & 0xff;
    if (address != 0xffff) {
        placeholder_memory[address + 1] = value >> 8;
    } else {
        std::cerr << "Warning: Attempted 16-bit write at 0xffff. Wrote only the lower byte.\n";
    }
}

} //namespace GameBoy
