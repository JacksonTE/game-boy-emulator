#pragma once

#include <bit>
#include <cstdint>
#include <filesystem>
#include <memory>
#include <string>

#include "central_processing_unit.h"
#include "game_cartridge_slot.h"
#include "internal_timer.h"
#include "memory_management_unit.h"

namespace GameBoyCore
{

static constexpr uint16_t ROM_TITLE_START = 0x0134;
static constexpr uint16_t ROM_TITLE_END = 0x0143;

class Emulator
{
public:
    Emulator();

    void reset_state(bool should_add_startup_machine_cycle);
    void set_post_boot_state();

    void step_central_processing_unit_single_instruction();
    RegisterFile<std::endian::native> get_register_file() const;
    void print_register_file_state() const;

    bool try_load_file_to_memory(std::filesystem::path file_path, bool is_bootrom_file, std::string &error_message);
    void unload_bootrom_from_memory_thread_safe();
    void unload_game_rom_from_memory_thread_safe();
    bool is_game_rom_loaded_in_memory_thread_safe() const;
    bool is_bootrom_loaded_in_memory_thread_safe() const;
    bool is_bootrom_mapped_in_memory() const;

    uint8_t read_byte_from_memory(uint16_t address) const;
    void write_byte_to_memory(uint16_t address, uint8_t value);
    void print_bytes_in_memory_range(uint16_t start_address, uint16_t end_address) const;

    void update_joypad_button_pressed_state_thread_safe(uint8_t button_flag_mask, bool new_button_pressed_state);
    void update_joypad_direction_pad_pressed_state_thread_safe(uint8_t direction_flag_mask, bool new_direction_pressed_state);

    uint8_t get_published_frame_buffer_index() const;
    std::unique_ptr<uint8_t[]> &get_pixel_frame_buffer(uint8_t index);

    std::string get_loaded_game_rom_title() const;

private:
    GameCartridgeSlot game_cartridge_slot{};
    InternalTimer internal_timer;
    PixelProcessingUnit pixel_processing_unit;
    std::unique_ptr<MemoryManagementUnit> memory_management_unit;
    CentralProcessingUnit central_processing_unit;

    void step_components_single_machine_cycle_to_sync_with_central_processing_unit();
    void request_interrupt(uint8_t interrupt_flag_mask);
};

} // namespace GameBoyCore
