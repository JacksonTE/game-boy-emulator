#pragma once

#include <array>
#include <cstdint>
#include <filesystem>
#include "pixel_processing_unit.h"
#include "timer.h"

namespace GameBoy
{

constexpr uint32_t MEMORY_SIZE = 0x10000;
constexpr uint16_t COLLECTIVE_ROM_BANK_SIZE = 0x8000;
constexpr uint16_t BOOTROM_SIZE = 0x100;

constexpr uint16_t HIGH_RAM_START = 0xff00;

constexpr uint8_t NUMBER_OF_INTERRUPT_TYPES = 5;
constexpr uint8_t INTERRUPT_FLAG_JOYPAD_MASK = 1 << 4;
constexpr uint8_t INTERRUPT_FLAG_SERIAL_MASK = 1 << 3;
constexpr uint8_t INTERRUPT_FLAG_TIMER_MASK = 1 << 2;

class MemoryManagementUnit
{
public:
    MemoryManagementUnit(Timer &timer_reference, PixelProcessingUnit &pixel_processing_unit_reference);
    virtual ~MemoryManagementUnit() = default;

    virtual void reset_state();
    void set_post_boot_state();
    bool try_load_file(uint16_t address, uint32_t number_of_bytes_to_load, std::filesystem::path file_path, bool is_bootrom_file);

    virtual uint8_t read_byte(uint16_t address) const;
    virtual void write_byte(uint16_t address, uint8_t value);

    void request_interrupt(uint8_t interrupt_flag_mask);
    bool is_interrupt_type_requested(uint8_t interrupt_flag_mask) const;
    bool is_interrupt_type_enabled(uint8_t interrupt_flag_mask) const;
    void clear_interrupt_flag_bit(uint8_t interrupt_flag_mask);

    void print_bytes_in_range(uint16_t start_address, uint16_t end_address) const;

private:
    std::unique_ptr<uint8_t[]> placeholder_memory{};
    std::unique_ptr<uint8_t[]> bootrom{};

    Timer &timer;
    PixelProcessingUnit &pixel_processing_unit;

    uint8_t interrupt_flag_if{0b11100000};
    uint8_t bootrom_status{};
    uint8_t interrupt_enable_ie{0b11100000};

    void wrote_to_read_only_address(uint16_t address) const;
};

} // namespace GameBoy
