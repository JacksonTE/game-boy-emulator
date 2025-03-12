#pragma once

#include <array>
#include <cstdint>

namespace GameBoy {

constexpr size_t memory_size = 0x10000;

class Memory {
public:
    uint8_t read_8(uint16_t address) const;
    void write_8(uint16_t address, uint8_t value);

    uint16_t read_16(uint16_t address) const;
    void write_16(uint16_t address, uint16_t value);

private:
    std::array<uint8_t, memory_size> placeholder_memory{};
};

} // namespace GameBoy
