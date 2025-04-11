#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iostream>
#include "emulator.h"
#include "machine_cycle_interaction.h"
#include "memory_management_unit.h"
#include "register_file.h"

namespace GameBoy {

Emulator::Emulator()
	: memory_interface{std::make_unique<MemoryManagementUnit>()},
      cpu{*memory_interface,
		  [this](MachineCycleInteraction _){
		      this->step_components_single_machine_cycle();
		  }} {
}

void Emulator::reset_state() {
	cpu.reset_state();
	memory_interface->reset_state();
}

void Emulator::set_post_boot_state() {
	cpu.set_post_boot_state();
	memory_interface->set_post_boot_state();
}

bool Emulator::try_load_bootrom(std::filesystem::path bootrom_path) {
	return try_load_file_to_memory(0x0000, BOOTROM_SIZE, bootrom_path, true);
}

void Emulator::step_cpu_single_instruction() {
	cpu.step_single_instruction();
}

RegisterFile<std::endian::native> Emulator::get_register_file() const {
	return cpu.get_register_file();
}

void Emulator::print_register_file_state() const {
	cpu.print_register_file_state();
}

uint8_t Emulator::read_byte_from_memory(uint16_t address) const {
	return memory_interface->read_byte(address);
}

void Emulator::write_byte_to_memory(uint16_t address, uint8_t value) {
	memory_interface->write_byte(address, value);
}

void Emulator::print_bytes_in_memory_range(uint16_t start_address, uint16_t end_address) const {
	memory_interface->print_bytes_in_range(start_address, end_address);
}

bool Emulator::try_load_file_to_memory(uint16_t address, uint32_t number_of_bytes_to_load, std::filesystem::path file_path, bool is_bootrom_file) {
	return memory_interface->try_load_file(address, number_of_bytes_to_load, file_path, is_bootrom_file);
}

void Emulator::step_components_single_machine_cycle() {
	cpu.step_single_machine_cycle();
	memory_interface->step_single_machine_cycle();
}

} // namespace GameBoy
