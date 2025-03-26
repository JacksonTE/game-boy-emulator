#pragma once

#include <array>
#include <cstdint>
#include <filesystem>

namespace GameBoy {

constexpr uint32_t MEMORY_SIZE = 0x10000;
constexpr uint16_t HIGH_RAM_START = 0xff00;

class Memory {
public:
    void print_range(uint16_t from_address, uint16_t to_address) const;

    uint8_t read_8(uint16_t address) const;
    void write_8(uint16_t address, uint8_t value);
    
    uint16_t read_16(uint16_t address) const;
    void write_16(uint16_t address, uint16_t value);

    bool try_write_range_from_file(uint16_t address, uint32_t number_of_bytes, std::filesystem::path file_path);

private:
    std::array<uint8_t, MEMORY_SIZE> placeholder_memory{};
};

} // namespace GameBoy
