#include <bit>
#include <cstdint>
#include <filesystem>

#include "emulator.h"
#include "console_output_utilities.h"
#include "memory_management_unit.h"

namespace GameBoyCore
{

Emulator::Emulator()
    : internal_timer{[this](uint8_t interrupt_flag_mask) { this->request_interrupt(interrupt_flag_mask); }},
      pixel_processing_unit{[this](uint8_t interrupt_flag_mask) { this->request_interrupt(interrupt_flag_mask); }},
      memory_management_unit{std::make_unique<MemoryManagementUnit>(game_cartridge_slot, internal_timer, pixel_processing_unit)},
      central_processing_unit{[this]() { this->step_components_single_machine_cycle_to_sync_with_central_processing_unit(); }, *memory_management_unit}
{
}

void Emulator::reset_state(bool should_add_startup_machine_cycle)
{
    internal_timer.reset_state();
    pixel_processing_unit.reset_state();
    memory_management_unit->reset_state();
    central_processing_unit.reset_state(should_add_startup_machine_cycle);
}

void Emulator::set_post_boot_state()
{
    internal_timer.set_post_boot_state();
    pixel_processing_unit.set_post_boot_state();
    memory_management_unit->set_post_boot_state();
    central_processing_unit.set_post_boot_state();
}

void Emulator::step_central_processing_unit_single_instruction()
{
    central_processing_unit.step_single_instruction();
}

RegisterFile<std::endian::native> Emulator::get_register_file() const
{
    return central_processing_unit.get_register_file();
}

void Emulator::print_register_file_state() const
{
    GameBoyCore::print_register_file_state(central_processing_unit.get_register_file());
}

bool Emulator::try_load_file_to_memory(std::filesystem::path file_path, bool is_bootrom_file, std::string &error_message)
{
    return memory_management_unit->try_load_file(file_path, is_bootrom_file, error_message);
}

bool Emulator::is_bootrom_loaded_in_memory_thread_safe() const
{
    return memory_management_unit->is_bootrom_loaded_thread_safe();
}

bool Emulator::is_game_rom_loaded_in_memory_thread_safe() const
{
    return memory_management_unit->is_game_rom_loaded_thread_safe();
}

void Emulator::unload_bootrom_from_memory_thread_safe()
{
    memory_management_unit->unload_bootrom_thread_safe();
}

void Emulator::unload_game_rom_from_memory_thread_safe()
{
    memory_management_unit->unload_game_rom_thread_safe();
}

uint8_t Emulator::read_byte_from_memory(uint16_t address) const
{
    return memory_management_unit->read_byte(address, true);
}

void Emulator::write_byte_to_memory(uint16_t address, uint8_t value)
{
    memory_management_unit->write_byte(address, value, false);
}

void Emulator::print_bytes_in_memory_range(uint16_t start_address, uint16_t end_address) const
{
    GameBoyCore::print_bytes_in_range([&](uint16_t address, bool is_access_for_oam_dma){ return memory_management_unit->read_byte(address, is_access_for_oam_dma); },
                                      start_address,
                                      end_address);
}

void Emulator::update_joypad_button_pressed_state_thread_safe(uint8_t button_flag_mask, bool new_button_pressed_state)
{
    memory_management_unit->update_joypad_button_pressed_state_thread_safe(button_flag_mask, new_button_pressed_state);
}

void Emulator::update_joypad_direction_pad_pressed_state_thread_safe(uint8_t direction_flag_mask, bool new_direction_pressed_state)
{
    memory_management_unit->update_joypad_direction_pad_pressed_state_thread_safe(direction_flag_mask, new_direction_pressed_state);
}

uint8_t Emulator::get_published_frame_buffer_index() const
{
    return pixel_processing_unit.get_published_frame_buffer_index();
}

std::unique_ptr<uint8_t[]> &Emulator::get_pixel_frame_buffer(uint8_t index)
{
    return pixel_processing_unit.get_pixel_frame_buffer(index);
}

std::string Emulator::get_loaded_game_rom_title() const
{
    std::string game_rom_title{};

    if (is_game_rom_loaded_in_memory_thread_safe())
    {
        game_rom_title.reserve(ROM_TITLE_END - ROM_TITLE_START + 1);

        for (uint16_t address = ROM_TITLE_START; address <= ROM_TITLE_END; address++)
        {
            const uint8_t title_byte = read_byte_from_memory(address);
            if (title_byte == 0x00)
            {
                break;
            }
            game_rom_title.push_back(static_cast<char>(title_byte));
        }
    }
    return game_rom_title;
}

void Emulator::step_components_single_machine_cycle_to_sync_with_central_processing_unit()
{
    internal_timer.step_single_machine_cycle();
    memory_management_unit->step_single_machine_cycle();
    pixel_processing_unit.step_single_machine_cycle();
}

void Emulator::request_interrupt(uint8_t interrupt_flag_mask)
{
    memory_management_unit->request_interrupt(interrupt_flag_mask);
}

} // namespace GameBoyCore
