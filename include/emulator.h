#pragma once

#include <bit>
#include <cstdint>
#include <filesystem>
#include "cpu.h"
#include "memory_management_unit.h"
#include "register_file.h"

namespace GameBoy {

class Emulator {
public:
	Emulator()
		: memory{std::make_unique<MemoryManagementUnit>()}, cpu{*memory} {}
	Emulator(std::unique_ptr<MemoryInterface> memory_interface)
		: memory{std::move(memory_interface)}, cpu{*memory} {}

	void reset_state();
	void set_post_boot_state();
	bool try_load_bootrom(std::filesystem::path bootrom_path);

	void execute_next_instruction();
	uint16_t get_program_counter() const;
	RegisterFile<std::endian::native> get_register_file() const;
	void update_register_file(const RegisterFile<std::endian::native> &new_register_values);
	void print_register_file_values() const;

	uint8_t read_byte_from_memory(uint16_t address) const;
	void write_byte_to_memory(uint16_t address, uint8_t value);
	void print_bytes_in_memory_range(uint16_t start_address, uint16_t end_address) const;
	bool try_load_file_to_memory(uint16_t address, uint32_t number_of_bytes_to_load, std::filesystem::path file_path, bool is_bootrom_file);

private:
	std::unique_ptr<MemoryInterface> memory;
	CPU cpu;
};

} // namespace GameBoy
