#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iostream>
#include "emulator.h"
#include "register_file.h"

namespace GameBoy {

void Emulator::reset_state() {
	cpu.reset_state();
	memory->reset_state();
}

void Emulator::set_post_boot_state() {
	cpu.set_post_boot_state();
	memory->set_post_boot_state();
}

bool Emulator::try_load_bootrom(std::filesystem::path bootrom_path) {
	return try_load_file_to_memory(0x0000, BOOTROM_SIZE, bootrom_path, true);
}

void Emulator::execute_next_instruction() {
	cpu.execute_next_instruction();
}

uint16_t Emulator::get_program_counter() const {
	return cpu.get_program_counter();
}

RegisterFile<std::endian::native> Emulator::get_register_file() const {
	return cpu.get_register_file();
}

void Emulator::update_register_file(const RegisterFile<std::endian::native> &new_register_values) {
	cpu.update_registers(new_register_values);
}

void Emulator::print_register_file_values() const {
	cpu.print_register_values();
}

uint8_t Emulator::read_byte_from_memory(uint16_t address) const {
	return memory->read_8(address);
}

void Emulator::write_byte_to_memory(uint16_t address, uint8_t value) {
	memory->write_8(address, value);
}

void Emulator::print_bytes_in_memory_range(uint16_t start_address, uint16_t end_address) const {
	memory->print_bytes_in_range(start_address, end_address);
}

bool Emulator::try_load_file_to_memory(uint16_t address, uint32_t number_of_bytes_to_load, std::filesystem::path file_path, bool is_bootrom_file) {
	return memory->try_load_file(address, number_of_bytes_to_load, file_path, is_bootrom_file);
}

} // namespace GameBoy
