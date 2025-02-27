#pragma once

#include <cstdint>

namespace GameBoy {
    class Memory {
    public:
        std::uint8_t read_8(std::uint16_t address) const;
        void write_8(std::uint16_t address, std::uint8_t value);

        std::uint16_t read_16(std::uint16_t address) const;
        void write_16(std::uint16_t address, std::uint16_t value);

    private:
        std::uint8_t placeholder_memory[65535]{};
    };
}
