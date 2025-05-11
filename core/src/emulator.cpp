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
      memory_management_unit{std::make_unique<MemoryManagementUnit>(internal_timer, pixel_processing_unit)},
      central_processing_unit{[this](MachineCycleOperation) { this->step_components_single_machine_cycle_to_sync_with_central_processing_unit(); }, *memory_management_unit}
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

bool Emulator::try_load_bootrom(std::filesystem::path bootrom_path)
{
    return try_load_file_to_memory(BOOTROM_SIZE, bootrom_path, true);
}

bool Emulator::is_frame_ready() const
{
    return pixel_processing_unit.is_frame_ready();
}

void Emulator::clear_frame_ready()
{
    pixel_processing_unit.clear_frame_ready();
}

std::unique_ptr<uint8_t[]> &Emulator::get_pixel_frame_buffer()
{
    return pixel_processing_unit.get_pixel_frame_buffer();
}

void Emulator::print_bytes_in_memory_range(uint16_t start_address, uint16_t end_address) const
{
    GameBoyCore::print_bytes_in_range(*memory_management_unit, start_address, end_address);
}

bool Emulator::try_load_file_to_memory(uint32_t number_of_bytes_to_load, std::filesystem::path file_path, bool is_bootrom_file)
{
    return memory_management_unit->try_load_file(number_of_bytes_to_load, file_path, is_bootrom_file);
}

uint8_t Emulator::read_byte_from_memory(uint16_t address) const
{
    return memory_management_unit->read_byte(address, false);
}

void Emulator::write_byte_to_memory(uint16_t address, uint8_t value)
{
    memory_management_unit->write_byte(address, value, false);
}

RegisterFile<std::endian::native> Emulator::get_register_file() const
{
    return central_processing_unit.get_register_file();
}

void Emulator::print_register_file_state() const
{
    GameBoyCore::print_register_file_state(central_processing_unit.get_register_file());
}

void Emulator::step_central_processing_unit_single_instruction()
{
    central_processing_unit.step_single_instruction();
}

void Emulator::step_components_single_machine_cycle_to_sync_with_central_processing_unit()
{
    internal_timer.step_single_machine_cycle();
    pixel_processing_unit.step_single_machine_cycle();
    memory_management_unit->step_single_machine_cycle();
}

void Emulator::request_interrupt(uint8_t interrupt_flag_mask)
{
    memory_management_unit->request_interrupt(interrupt_flag_mask);
}

} // namespace GameBoyCore
