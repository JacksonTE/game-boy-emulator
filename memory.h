#pragma once

#include <cstdint>

namespace GameBoy {
    class Memory {
    public:
        std::uint8_t operator[](std::uint16_t address) const;
        std::uint8_t& operator[](std::uint16_t address);

        std::uint16_t read16(std::uint16_t address) const;
        void write16(std::uint16_t address, std::uint16_t value);

    private:
        std::uint8_t placeholderMemory[65535]{};
    };
}
