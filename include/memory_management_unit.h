#pragma once

#include <array>
#include <cstdint>
#include <filesystem>
#include <functional>

namespace GameBoy {

constexpr uint32_t MEMORY_SIZE = 0x10000;
constexpr uint16_t COLLECTIVE_ROM_BANK_SIZE = 0x8000;
constexpr uint16_t BOOTROM_SIZE = 0x100;

constexpr uint16_t HIGH_RAM_START = 0xff00;
constexpr uint16_t BOOTROM_STATUS_ADDRESS = 0xff50;

class MemoryManagementUnit {
public:
    MemoryManagementUnit();

    virtual void reset_state();
    void set_post_boot_state();
    bool try_load_file(uint16_t address, uint32_t number_of_bytes_to_load, std::filesystem::path file_path, bool is_bootrom_file);

    virtual uint8_t read_byte(uint16_t address) const;
    virtual void write_byte(uint16_t address, uint8_t value);

    void print_bytes_in_range(uint16_t start_address, uint16_t end_address) const;

private:
    std::unique_ptr<uint8_t[]> placeholder_memory;
    std::unique_ptr<uint8_t[]> bootrom{};
};

} // namespace GameBoy
