#pragma once

#include <cstdint>
#include <filesystem>
#include "cpu.h"
#include "memory.h"

namespace GameBoy {

class GameBoy {
public:
	GameBoy() : memory{}, cpu{memory} {}
	void initialize();

	void execute_next_cpu_instruction();
	void print_cpu_register_values() const;
	uint16_t get_cpu_program_counter() const;

	void print_bytes_in_memory_range(uint16_t start_address, uint16_t end_address) const;
	bool try_load_file_to_memory(uint16_t address, uint32_t number_of_bytes_to_load, std::filesystem::path file_path, bool is_bootrom_file);

private:
	CPU cpu;
	Memory memory;
};

} // namespace GameBoy
