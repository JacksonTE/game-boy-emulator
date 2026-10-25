#include <algorithm>
#include <filesystem>
#include <gtest/gtest.h>
#include <vector>

#include "emulator.h"

static std::filesystem::path get_test_directory_path()
{
    return std::filesystem::path(PROJECT_ROOT) / "tests" / "data" / "gbmicrotest" / "bin";
}

static std::vector<std::filesystem::path> get_test_rom_paths_in_directory(const std::filesystem::path& directory)
{
    if (!std::filesystem::exists(directory))
    {
        return { directory };
    }
    std::vector<std::filesystem::path> test_rom_paths;

    for (const auto& entry : std::filesystem::directory_iterator(directory))
    {
        const bool is_enabled_test =
            entry.path().filename() != "000-oam_lock.gb" &&
            entry.path().filename() != "000-write_to_x8000.gb" &&
            entry.path().filename() != "001-vram_unlocked.gb" &&
            entry.path().filename() != "002-vram_locked.gb" &&
            entry.path().filename() != "004-tima_boot_phase.gb" &&
            entry.path().filename() != "004-tima_cycle_timer.gb" &&
            entry.path().filename() != "007-lcd_on_stat.gb" &&
            entry.path().filename() != "400-dma.gb" &&
            entry.path().filename() != "500-scx-timing.gb" &&
            entry.path().filename() != "800-ppu-latch-scx.gb" &&
            entry.path().filename() != "801-ppu-latch-scy.gb" &&
            entry.path().filename() != "802-ppu-latch-tileselect.gb" &&
            entry.path().filename() != "803-ppu-latch-bgdisplay.gb" &&
            entry.path().filename() != "audio_testbench.gb" &&
            entry.path().filename() != "cpu_bus_1.gb" &&
            entry.path().filename() != "dma_basic.gb" &&
            entry.path().filename() != "flood_vram.gb" &&
            entry.path().filename() != "halt_op_dupe_delay.gb" &&
            entry.path().filename() != "lcdon_write_timing.gb" &&
            entry.path().filename() != "line_153_lyc_b.gb" &&
            entry.path().filename() != "ly_while_lcd_off.gb" &&
            entry.path().filename() != "mbc1_rom_banks.gb" &&
            entry.path().filename() != "minimal.gb" &&
            entry.path().filename() != "mode2_stat_int_to_oam_unlock.gb" &&
            entry.path().filename() != "oam_sprite_trashing.gb" &&
            entry.path().filename() != "poweron.gb" &&
            entry.path().filename() != "ppu_scx_vs_bgp.gb" &&
            entry.path().filename() != "ppu_sprite_testbench.gb" &&
            entry.path().filename() != "ppu_spritex_vs_scx.gb" &&
            entry.path().filename() != "ppu_win_vs_wx.gb" &&
            entry.path().filename() != "ppu_wx_early.gb" &&
            entry.path().filename() != "stat_write_glitch_l154_d.gb" &&
            entry.path().filename() != "temp.gb" &&
            entry.path().filename() != "toggle_lcdc.gb" &&
            entry.path().filename() != "wave_write_to_0xC003.gb";
        if (entry.is_regular_file() && entry.path().extension() == ".gb" && is_enabled_test)
        {
            test_rom_paths.push_back(entry.path());
        }
    }
    std::sort(test_rom_paths.begin(), test_rom_paths.end());
    return test_rom_paths;
}

class GbmicroTest : public testing::TestWithParam<std::filesystem::path>
{
protected:
    GameBoyEmulator::Emulator game_boy_emulator;
    std::string error_message{};

    void SetUp() override
    {
        ASSERT_TRUE(std::filesystem::exists(GetParam())) << "ROM file not found: " << GetParam();
        ASSERT_TRUE(game_boy_emulator.try_to_load_file_to_memory(GetParam(), GameBoyEmulator::FileType::GameROM, error_message)) << error_message;
        game_boy_emulator.reset_state();
    }
};

TEST_P(GbmicroTest, TestRom)
{
    const std::filesystem::path test_rom_path = GetParam();
    SCOPED_TRACE("Test ROM: " + test_rom_path.string());

    constexpr size_t MAX_INSTRUCTIONS_BEFORE_TIMEOUT = 1'000'000;
    bool did_test_succeed = false;

    for (size_t _ = 0; _ < MAX_INSTRUCTIONS_BEFORE_TIMEOUT; _++)
    {
        game_boy_emulator.execute_next_instruction();

        const uint8_t test_result_byte = game_boy_emulator.read_byte_from_memory(0xFF80);
        const uint8_t test_expected_result_byte = game_boy_emulator.read_byte_from_memory(0xFF81);
        const uint8_t test_pass_fail_byte = game_boy_emulator.read_byte_from_memory(0xFF82);

        if (test_pass_fail_byte == 0xFF)
        {
            FAIL() << "Test failed with result 0x" << std::hex << static_cast<int>(test_result_byte) << ". Expected result was 0x" << static_cast<int>(test_expected_result_byte) << "\n";
        }
        else if (test_pass_fail_byte == 0x01)
        {
            did_test_succeed = true;
            break;
        }
    }
    ASSERT_TRUE(did_test_succeed) << "Test didn't reach a finished state within " << MAX_INSTRUCTIONS_BEFORE_TIMEOUT << " instructions"
        << ". PC=0x" << std::hex << game_boy_emulator.get_register_file().program_counter
        << ", A=0x" << static_cast<int>(game_boy_emulator.get_register_file().A)
        << ", FF80=0x" << static_cast<int>(game_boy_emulator.read_byte_from_memory(0xFF80))
        << ", FF81=0x" << static_cast<int>(game_boy_emulator.read_byte_from_memory(0xFF81))
        << ", FF82=0x" << static_cast<int>(game_boy_emulator.read_byte_from_memory(0xFF82));
}

INSTANTIATE_TEST_SUITE_P(
    GbmicroTests,
    GbmicroTest,
    testing::ValuesIn(get_test_rom_paths_in_directory(get_test_directory_path())),
    [](auto info)
    {
        std::string test_rom_file_name = info.param.stem().string();
        std::replace(test_rom_file_name.begin(), test_rom_file_name.end(), '-', '_');
        return test_rom_file_name;
    });
