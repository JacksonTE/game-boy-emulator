#include "memory.h"

namespace GameBoy {
    std::uint8_t Memory::read8(std::uint16_t address) const {
        return placeholderMemory[address];
    }

    void Memory::write8(std::uint16_t address, std::uint8_t value) {
        placeholderMemory[address] = value;
    }

    std::uint16_t Memory::read16(std::uint16_t address) const {
        // Little-endian read, first byte is least significant
        return placeholderMemory[address] | (placeholderMemory[address + 1] << 8);
    }

    void Memory::write16(std::uint16_t address, std::uint16_t value) {
        // Little-endian write, first byte is least significant
        placeholderMemory[address] = value & 0xFF;
        placeholderMemory[address + 1] = value >> 8;
    };
}
