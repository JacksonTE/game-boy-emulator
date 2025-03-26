#include "game_boy.h"

int main() {
    GameBoy::GameBoy game_boy{};

    std::filesystem::path bootrom_path = std::filesystem::path("..")/ ".." / ".." / "assets" / "bootrom_dmg.bin";
    game_boy.load_file_to_memory(0x00, 0x100, bootrom_path);
    game_boy.print_memory_range(0x00, 0xff);
    
    for (int _ = 0; _ < 0x100; _++) {
        game_boy.execute_next_instruction();
    }
    game_boy.print_cpu_register_values();
    return 0;
}
