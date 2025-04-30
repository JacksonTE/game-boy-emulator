#include <algorithm>
#include <filesystem>
#include <gtest/gtest.h>
#include <vector>

#include "emulator.h"

static std::filesystem::path get_acceptance_test_directory_path()
{
    return std::filesystem::path(PROJECT_ROOT) / "tests" / "data" / "mooneye-test-suite" / "mts-20240926-1737-443f6e1" / "acceptance";
}

static std::vector<std::filesystem::path> get_test_rom_paths_in_directory(const std::filesystem::path &directory)
{
    if (!std::filesystem::exists(directory))
        return { directory };

    std::vector<std::filesystem::path> test_rom_paths;

    for (const auto &entry : std::filesystem::directory_iterator(directory))
    {
        if (entry.is_regular_file() && entry.path().extension() == ".gb" &&
            entry.path().filename() != "unused_hwio-GS.gb" &&
            (directory.filename() != "acceptance" ||
             //entry.path().filename() == "add_sp_e_timing.gb" ||
             //entry.path().filename() == "boot_div-dmgABCmgb.gb" ||
             //entry.path().filename() == "boot_hwio-dmgABCmgb.gb" ||
             entry.path().filename() == "boot_regs-dmgABC.gb" ||
             //entry.path().filename() == "call_cc_timing.gb" ||
             //entry.path().filename() == "call_cc_timing2.gb" ||
             //entry.path().filename() == "call_timing.gb" ||
             //entry.path().filename() == "call_timing2.gb" ||
             entry.path().filename() == "di_timing-GS.gb" ||
             entry.path().filename() == "div_timing.gb" ||
             entry.path().filename() == "ei_sequence.gb" ||
             entry.path().filename() == "ei_timing.gb" ||
             entry.path().filename() == "halt_ime0_ei.gb" ||
             //entry.path().filename() == "halt_ime0_nointr_timing.gb" ||
             entry.path().filename() == "halt_ime1_timing.gb" ||
             entry.path().filename() == "halt_ime1_timing2-GS.gb" ||
             entry.path().filename() == "if_ie_registers.gb" ||
             entry.path().filename() == "intr_timing.gb" ||
             //entry.path().filename() == "jp_cc_timing.gb" ||
             //entry.path().filename() == "jp_timing.gb" ||
             //entry.path().filename() == "ld_hl_sp_e_timing.gb" ||
             //entry.path().filename() == "oam_dma_restart.gb" ||
             //entry.path().filename() == "oam_dma_start.gb" ||
             //entry.path().filename() == "oam_dma_timing.gb" ||
             entry.path().filename() == "pop_timing.gb" ||
             //entry.path().filename() == "push_timing.gb" ||
             entry.path().filename() == "rapid_di_ei.gb"
             //entry.path().filename() == "ret_cc_timing.gb" ||
             //entry.path().filename() == "ret_timing.gb" ||
             //entry.path().filename() == "reti_intr_timing.gb" ||
             //entry.path().filename() == "reti_timing.gb" ||
             //entry.path().filename() == "rst_timing.gb"
             ))
        {
            test_rom_paths.push_back(entry.path());
        }
    }
    std::sort(test_rom_paths.begin(), test_rom_paths.end());
    return test_rom_paths;
}

class MooneyeTest : public testing::TestWithParam<std::filesystem::path>
{
protected:
    GameBoy::Emulator game_boy_emulator;

    void SetUp() override
    {
        ASSERT_TRUE(std::filesystem::exists(GetParam())) << "ROM file not found: " << GetParam();
        game_boy_emulator.reset_state();
        game_boy_emulator.try_load_file_to_memory(32768, GetParam(), false);
        game_boy_emulator.set_post_boot_state();
    }
};

TEST_P(MooneyeTest, TestRom)
{
    const std::filesystem::path test_rom_path = GetParam();
    SCOPED_TRACE("Test ROM: " + test_rom_path.string());

    constexpr size_t max_instructions_before_timeout = 10'000'000;
    bool did_test_succeed = false;

    for (size_t _ = 0; _ < max_instructions_before_timeout; _++)
    {
        game_boy_emulator.step_cpu_single_instruction();
        auto r = game_boy_emulator.get_register_file();

        if (r.b == 0x42 && r.c == 0x42 && r.d == 0x42 && r.e == 0x42 && r.h == 0x42 && r.l == 0x42)
        {
            FAIL();
        }
        else if (r.b == 3 && r.c == 5 && r.d == 8 && r.e == 13 && r.h == 21 && r.l == 34)
        {
            did_test_succeed = true;
            break;
        }
    }
    ASSERT_TRUE(did_test_succeed) << "Test didn't reach a finished state within " << max_instructions_before_timeout << " instructions";
}

INSTANTIATE_TEST_SUITE_P
(
    MooneyeAcceptanceTestsBits,
    MooneyeTest,
    testing::ValuesIn(get_test_rom_paths_in_directory(get_acceptance_test_directory_path() / "bits")),
    [](auto info) { return info.param.stem().string(); }
);

INSTANTIATE_TEST_SUITE_P
(
    MooneyeAcceptanceTestsInstructions,
    MooneyeTest,
    testing::ValuesIn(get_test_rom_paths_in_directory(get_acceptance_test_directory_path() / "instr")),
    [](auto info) { return info.param.stem().string(); }
);

INSTANTIATE_TEST_SUITE_P
(
    MooneyeAcceptanceTestsInterrupts,
    MooneyeTest,
    testing::ValuesIn(get_test_rom_paths_in_directory(get_acceptance_test_directory_path() / "interrupts")),
    [](auto info) { return info.param.stem().string(); }
);

INSTANTIATE_TEST_SUITE_P
(
    MooneyeAcceptanceTestsObjectAttributeMemoryDirectMemoryAccess,
    MooneyeTest,
    testing::ValuesIn(get_test_rom_paths_in_directory(get_acceptance_test_directory_path() / "oam_dma")),
    [](auto info)
    {
        std::string test_rom_file_name = info.param.stem().string();
        std::replace(test_rom_file_name.begin(), test_rom_file_name.end(), '-', '_');
        return test_rom_file_name;
    }
);

INSTANTIATE_TEST_SUITE_P
(
    MooneyeAcceptanceTestsPixelProcessingUnit,
    MooneyeTest,
    testing::ValuesIn(get_test_rom_paths_in_directory(get_acceptance_test_directory_path() / "ppu")),
    [](auto info) 
    { 
        std::string test_rom_file_name = info.param.stem().string();
        std::replace(test_rom_file_name.begin(), test_rom_file_name.end(), '-', '_');
        return test_rom_file_name;
    }
);

INSTANTIATE_TEST_SUITE_P
(
    MooneyeAcceptanceTestsTimer,
    MooneyeTest,
    testing::ValuesIn(get_test_rom_paths_in_directory(get_acceptance_test_directory_path() / "timer")),
    [](auto info) { return info.param.stem().string(); }
);

INSTANTIATE_TEST_SUITE_P
(
    MooneyeAcceptanceTestsMiscellaneous,
    MooneyeTest,
    testing::ValuesIn(get_test_rom_paths_in_directory(get_acceptance_test_directory_path())),
    [](auto info)
    {
        std::string test_rom_file_name = info.param.stem().string();
        std::replace(test_rom_file_name.begin(), test_rom_file_name.end(), '-', '_');
        return test_rom_file_name;
    }
);
