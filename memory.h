#pragma once

#include <cstdint>

namespace GameBoy {

class Memory {
public:
    uint8_t read_8(uint16_t address) const;
    void write_8(uint16_t address, uint8_t value);

    uint16_t read_16(uint16_t address) const;
    void write_16(uint16_t address, uint16_t value);

private:
    uint8_t placeholder_memory[65535]{};
};

} // namespace GameBoy
