#pragma once

#include <cstdint>
#include <filesystem>
#include "cpu.h"
#include "memory.h"

namespace GameBoy {

class GameBoy {
public:
	GameBoy() : memory{}, cpu{memory} {}
	void print_cpu_register_values() const;
	void execute_next_instruction();
	bool load_file_to_memory(uint16_t starting_memory_address, uint32_t number_of_bytes_to_load, std::filesystem::path file_path);
	void print_memory_range(uint16_t from_memory_address, uint16_t to_memory_address) const;

private:
	CPU cpu;
	Memory memory;
};

} // namespace GameBoy
