#include <fstream>
#include <iostream>
#include "game_boy.h"

namespace GameBoy {

void GameBoy::initialize() { // TODO maybe make apart of constructor
	memory.allocate_boot_rom();
}

void GameBoy::execute_next_cpu_instruction() {
	cpu.execute_next_instruction();
}

void GameBoy::print_cpu_register_values() const {
	cpu.print_register_values();
}

uint16_t GameBoy::get_cpu_program_counter() const { // TODO remove eventually
	return cpu.get_program_counter();
}

void GameBoy::print_bytes_in_memory_range(uint16_t start_address, uint16_t end_address) const {
	memory.print_bytes_in_range(start_address, end_address);
}

bool GameBoy::try_load_file_to_memory(uint16_t address, uint32_t number_of_bytes_to_load, std::filesystem::path file_path, bool is_bootrom_file) {
	return memory.try_load_file(address, number_of_bytes_to_load, file_path, is_bootrom_file);
}

} // namespace GameBoy