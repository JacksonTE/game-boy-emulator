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

    std::filesystem::path test_rom_path = std::filesystem::path(PROJECT_ROOT) /
        "tests" / "data" / "mooneye-test-suite" / "mts-20240926-1737-443f6e1" / "acceptance" / "ppu" / "hblank_ly_scx_timing-GS.gb";
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
        auto r = game_boy_emulator.get_register_file();
        if (r.b == 42 && r.c == 42 && r.d == 42 && r.e == 42 && r.h == 42 && r.l == 42) {
            std::cout << "test failed" << "\n";
            break;
        }
        if (r.b == 3 && r.c == 5 && r.d == 8 && r.e == 13 && r.h == 21 && r.l == 34) {
            std::cout << "test passed" << "\n";
            break;
        }
        game_boy_emulator.step_cpu_single_instruction();
    }
    return 0;
}
