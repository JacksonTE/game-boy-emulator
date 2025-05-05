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

    std::filesystem::path test_rom_path = std::filesystem::path(PROJECT_ROOT) /
        "tests" / "data" / "gbmicrotest" / "bin" / "win0_scx3_a.gb";
    game_boy_emulator.try_load_file_to_memory(2 * GameBoy::ROM_BANK_SIZE, test_rom_path, false);

    //if (bootrom_path.empty())
    //{
    //    game_boy_emulator.set_post_boot_state();
    //}
    //else
    //{
    //    if (!game_boy_emulator.try_load_bootrom(bootrom_path))
    //    {
    //        std::cerr << "Error: unable to initialize Game Boy with provided bootrom path, exiting.\n";
    //        return 1;
    //    }
    //}
    game_boy_emulator.set_post_boot_state();

    while (true)
    {
        const uint8_t test_result_byte = game_boy_emulator.read_byte_from_memory(0xff80);
        const uint8_t test_expected_result_byte = game_boy_emulator.read_byte_from_memory(0xff81);
        const uint8_t test_pass_fail_byte = game_boy_emulator.read_byte_from_memory(0xff82);

        if (test_pass_fail_byte == 0xff)
        {
            std::cout << "Test failed with result " << static_cast<int>(test_result_byte) << ". Expected result was " << static_cast<int>(test_expected_result_byte) << "\n";
            break;
        }
        else if (test_pass_fail_byte == 0x01)
        {
            std::cout << "Test passed with result " << static_cast<int>(test_result_byte) << "\n";
            break;
        }
        game_boy_emulator.step_cpu_single_instruction();
    }
    return 0;
}
