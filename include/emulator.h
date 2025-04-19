#pragma once

#include <bit>
#include <cstdint>
#include <filesystem>
#include <functional>
#include "cpu.h"
#include "register_file.h"
#include "timer.h"

namespace GameBoy
{

class Emulator
{
public:
    Emulator();

    void reset_state();
    void set_post_boot_state();
    bool try_load_bootrom(std::filesystem::path bootrom_path);

    uint8_t read_byte_from_memory(uint16_t address) const;
    void write_byte_to_memory(uint16_t address, uint8_t value);
    void print_bytes_in_memory_range(uint16_t start_address, uint16_t end_address) const;
    bool try_load_file_to_memory(uint16_t address, uint32_t number_of_bytes_to_load, std::filesystem::path file_path, bool is_bootrom_file);

    void step_cpu_single_instruction();
    RegisterFile<std::endian::native> get_register_file() const;
    void print_register_file_state() const;

private:
    Timer timer;
    PixelProcessingUnit pixel_processing_unit;
    std::unique_ptr<MemoryManagementUnit> memory_interface;
    CPU cpu;

    void request_interrupt(uint8_t interrupt_flag_mask);
    void step_components_single_machine_cycle();
};

} // namespace GameBoy
