#include <bit>
#include <cstdint>
#include <filesystem>

#include "emulator.h"
#include "console_output_utilities.h"
#include "memory_management_unit.h"

namespace GameBoy
{

Emulator::Emulator()
    : timer{[this](uint8_t interrupt_flag_mask) { this->request_interrupt(interrupt_flag_mask); }},
      pixel_processing_unit{[this](uint8_t interrupt_flag_mask) { this->request_interrupt(interrupt_flag_mask); }},
      memory_interface{std::make_unique<MemoryManagementUnit>(timer, pixel_processing_unit)},
      central_processing_unit{[this](MachineCycleOperation) { this->step_components_single_machine_cycle(); }, *memory_interface}
{
}

void Emulator::reset_state()
{
    timer.reset_state();
    pixel_processing_unit.reset_state();
    memory_interface->reset_state();
    central_processing_unit.reset_state();
}

void Emulator::set_post_boot_state()
{
    timer.set_post_boot_state();
    pixel_processing_unit.set_post_boot_state();
    memory_interface->set_post_boot_state();
    central_processing_unit.set_post_boot_state();
}

bool Emulator::try_load_bootrom(std::filesystem::path bootrom_path)
{
    return try_load_file_to_memory(BOOTROM_SIZE, bootrom_path, true);
}

void Emulator::print_bytes_in_memory_range(uint16_t start_address, uint16_t end_address) const
{
    print_bytes_in_range(*memory_interface, start_address, end_address);
}

bool Emulator::try_load_file_to_memory(uint32_t number_of_bytes_to_load, std::filesystem::path file_path, bool is_bootrom_file)
{
    return memory_interface->try_load_file(number_of_bytes_to_load, file_path, is_bootrom_file);
}

uint8_t Emulator::read_byte_from_memory(uint16_t address) const
{
    return memory_interface->read_byte(address);
}

void Emulator::write_byte_to_memory(uint16_t address, uint8_t value)
{
    memory_interface->write_byte(address, value);
}

RegisterFile<std::endian::native> Emulator::get_register_file() const
{
    return central_processing_unit.get_register_file();
}

void Emulator::print_register_file_state() const
{
    GameBoy::print_register_file_state(central_processing_unit.get_register_file());
}

void Emulator::step_cpu_single_instruction()
{
    central_processing_unit.step_single_instruction();
}

void Emulator::step_components_single_machine_cycle()
{
    timer.step_single_machine_cycle();
    pixel_processing_unit.step_single_machine_cycle();
}

void Emulator::request_interrupt(uint8_t interrupt_flag_mask)
{
    memory_interface->request_interrupt(interrupt_flag_mask);
}

} // namespace GameBoy
