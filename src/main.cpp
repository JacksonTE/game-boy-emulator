#include <iostream>
#include <string_view>
#include "game_boy.h"

int main(int argc, char *argv[]) {
    GameBoy::GameBoy game_boy{};
    std::filesystem::path bootrom_path{};

    for (int i = 1; i < argc; i++) {
        std::string_view argument{argv[i]};

        if (argument == "--bootrom") {
            if (i + 1 < argc) {
                bootrom_path = std::string{argv[i + 1]};
            }
            else {
                std::cerr << "Error: --bootrom requires a file path.\n";
                return 1;
            }
        }
    }

    std::filesystem::path test_rom_path = std::filesystem::path("..") / ".." / ".." / "bootrom" / "tetris.gb";
    game_boy.try_load_file_to_memory(0x0000, GameBoy::COLLECTIVE_ROM_BANK_SIZE, test_rom_path, false);

    if (bootrom_path.empty()) {
        game_boy.set_post_boot_state();
    }
    else {
        if (!game_boy.try_load_bootrom(bootrom_path)) {
            std::cerr << "Error: unable to initialize Game Boy with provided bootrom path, exiting.\n";
            return 1;
        }
    }

    while (game_boy.get_program_counter() < 0x100) {
        game_boy.execute_next_instruction();
    }
    game_boy.print_register_file_values();
    return 0;
}
