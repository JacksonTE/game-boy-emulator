#include <iostream>
#include "game_boy.h"

int main() {
    GameBoy::GameBoy game_boy{};
    game_boy.initialize();

    std::filesystem::path bootrom_path = std::filesystem::path("..")/ ".." / ".." / "assets" / "boot_rom_dmg.bin";
    game_boy.try_load_file_to_memory(0x0000, GameBoy::BOOT_ROM_SIZE, bootrom_path, true);

    std::filesystem::path tetris_rom_path = std::filesystem::path("..")/ ".." / ".." / "assets" / "tetris.gb";
    game_boy.try_load_file_to_memory(0x0000, GameBoy::COLLECTIVE_ROM_BANK_SIZE, tetris_rom_path, false);
    
    while (game_boy.get_cpu_program_counter() < 0x100) {
        game_boy.execute_next_cpu_instruction();
    }

    std::cout << "header checksum (non-zero means H and C should be set):\n";
    game_boy.print_bytes_in_memory_range(0x14d, 0x14d);
    game_boy.print_cpu_register_values();

    return 0;
}
