#include <fstream>
#include <iostream>
#include "game_boy.h"

namespace GameBoy {

void GameBoy::print_cpu_register_values() const {
	cpu.print_register_values();
}

void GameBoy::execute_next_instruction() {
	cpu.execute_next_instruction();
}

bool GameBoy::load_file_to_memory(uint16_t starting_memory_address, uint32_t number_of_bytes_to_load, std::filesystem::path file_path) {
	return memory.try_write_range_from_file(starting_memory_address, number_of_bytes_to_load, file_path);
}

void GameBoy::print_memory_range(uint16_t from_memory_address, uint16_t to_memory_address) const {
	memory.print_range(from_memory_address, to_memory_address);
}

} // namespace GameBoy