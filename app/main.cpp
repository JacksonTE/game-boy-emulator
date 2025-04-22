#include <filesystem>
#include <iostream>
#include <string_view>

#include "emulator.h"

int main(int argc, char *argv[])
{
    GameBoy::Emulator game_boy_emulator{};
    std::filesystem::path bootrom_path{};

    for (int i = 1; i < argc; i++)
    {
        std::string_view argument{argv[i]};

        if (argument == "--bootrom")
        {
            if (i + 1 < argc)
            {
                bootrom_path = std::filesystem::path(PROJECT_ROOT) / "bootrom" / std::string{argv[i + 1]};
            }
            else
            {
                std::cerr << "Error: The '--bootrom' option requires a bootrom filename argument. "
                          << "Please provide the filename (including its extension) for a bootrom file located in the 'bootrom/' directory.\n";
                return 1;
            }
        }
    }

    // TODO FIX REMAINING:
    // Failing intr_1_2_timing-GS.gb, intr_2_mode0_timing_sprites.gb, intr_2_0_timing.gb 
    std::filesystem::path test_rom_path = std::filesystem::path(PROJECT_ROOT) /
        //"bootrom" / "Tetris (JUE) (V1.1) [!].gb";
        "tests" / "data" / "mooneye-test-suite" / "mts-20240926-1737-443f6e1" / "acceptance" / "ppu" / "hblank_ly_scx_timing-GS.gb";
    game_boy_emulator.try_load_file_to_memory(2 * GameBoy::ROM_BANK_SIZE, test_rom_path, false);

    if (bootrom_path.empty())
    {
        game_boy_emulator.set_post_boot_state();
    }
    else
    {
        if (!game_boy_emulator.try_load_bootrom(bootrom_path))
        {
            std::cerr << "Error: unable to initialize Game Boy with provided bootrom path, exiting.\n";
            return 1;
        }
    }

    while (true)
    {
        auto r = game_boy_emulator.get_register_file();

        if (r.b == 0x42 && r.c == 0x42 && r.d == 0x42 && r.e == 0x42 && r.h == 0x42 && r.l == 0x42)
        {
            std::cout << "test failed" << "\n";
            break;
        }
        else if (r.b == 3 && r.c == 5 && r.d == 8 && r.e == 13 && r.h == 21 && r.l == 34)
        {
            std::cout << "test passed" << "\n";
            break;
        }
        game_boy_emulator.step_cpu_single_instruction();
    }
    return 0;
}
