#include <filesystem>
#include <iostream>
#include <string_view>
#include "emulator.h"

int main(int argc, char *argv[]) {
    GameBoy::Emulator game_boy_emulator{};
    std::filesystem::path bootrom_path{};

    for (int i = 1; i < argc; i++) {
        std::string_view argument{argv[i]};

        if (argument == "--bootrom") {
            if (i + 1 < argc) {
                bootrom_path = std::filesystem::path(PROJECT_ROOT) / "bootrom" / std::string{argv[i + 1]};
            }
            else {
                std::cerr << "Error: The '--bootrom' option requires a bootrom filename argument. "
                          << "Please provide the filename (including its extension) for a bootrom file located in the 'bootrom/' directory.\n";
                return 1;
            }
        }
    }

    std::filesystem::path test_rom_path = std::filesystem::path(PROJECT_ROOT) / "bootrom" / "cpu_instrs" / "individual" / "02-interrupts.gb";
    game_boy_emulator.try_load_file_to_memory(0x0000, GameBoy::COLLECTIVE_ROM_BANK_SIZE, test_rom_path, false);

    if (bootrom_path.empty()) {
        game_boy_emulator.set_post_boot_state();
    }
    else {
        if (!game_boy_emulator.try_load_bootrom(bootrom_path)) {
            std::cerr << "Error: unable to initialize Game Boy with provided bootrom path, exiting.\n";
            return 1;
        }
    }

    while (true) {
        game_boy_emulator.execute_next_instruction();
    }
    //game_boy_emulator.print_register_file_state();
    return 0;
}
