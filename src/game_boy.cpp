#include <fstream>
#include <iostream>
#include "game_boy.h"

namespace GameBoy {

void GameBoy::reset() {
	cpu.reset();
	memory.reset();
}

void GameBoy::set_post_boot_state() {
	cpu.set_post_boot_state();
	memory.set_post_boot_state();
}

bool GameBoy::try_load_bootrom(std::filesystem::path bootrom_path) {
	return try_load_file_to_memory(0x0000, BOOTROM_SIZE, bootrom_path, true);
}

void GameBoy::execute_next_instruction() {
	cpu.execute_next_instruction();
}

uint16_t GameBoy::get_program_counter() const {
	return cpu.get_program_counter();
}

RegisterFile<std::endian::native> GameBoy::get_register_file() const {
	return cpu.get_register_file();
}

void GameBoy::update_register_file(const RegisterFile<std::endian::native> &new_register_values) {
	cpu.update_registers(new_register_values);
}

void GameBoy::print_register_file_values() const {
	cpu.print_register_values();
}

uint8_t GameBoy::read_byte_from_memory(uint16_t address) const {
	return memory.read_8(address);
}

void GameBoy::write_byte_to_memory(uint16_t address, uint8_t value) {
	memory.write_8(address, value);
}

void GameBoy::print_bytes_in_memory_range(uint16_t start_address, uint16_t end_address) const {
	memory.print_bytes_in_range(start_address, end_address);
}

bool GameBoy::try_load_file_to_memory(uint16_t address, uint32_t number_of_bytes_to_load, std::filesystem::path file_path, bool is_bootrom_file) {
	return memory.try_load_file(address, number_of_bytes_to_load, file_path, is_bootrom_file);
}

} // namespace GameBoy
