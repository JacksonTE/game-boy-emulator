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
        return { directory };

    std::vector<std::filesystem::path> test_rom_paths;

    for (const auto& entry : std::filesystem::directory_iterator(directory))
    {
        if (entry.is_regular_file() && entry.path().extension() == ".gb" &&
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
            entry.path().filename() != "hblank_int_di_timing_b.gb" &&
            entry.path().filename() != "hblank_int_if_a.gb" &&
            entry.path().filename() != "hblank_int_l0.gb" &&
            entry.path().filename() != "hblank_int_l1.gb" &&
            entry.path().filename() != "hblank_int_l2.gb" &&
            entry.path().filename() != "hblank_int_scx0.gb" &&
            entry.path().filename() != "hblank_int_scx1.gb" &&
            entry.path().filename() != "hblank_int_scx1_if_b.gb" &&
            entry.path().filename() != "hblank_int_scx1_if_c.gb" &&
            entry.path().filename() != "hblank_int_scx1_if_d.gb" &&
            entry.path().filename() != "hblank_int_scx1_nops_a.gb" &&
            entry.path().filename() != "hblank_int_scx1_nops_b.gb" &&
            entry.path().filename() != "hblank_int_scx2.gb" &&
            entry.path().filename() != "hblank_int_scx2_if_b.gb" &&
            entry.path().filename() != "hblank_int_scx2_if_c.gb" &&
            entry.path().filename() != "hblank_int_scx2_if_d.gb" &&
            entry.path().filename() != "hblank_int_scx2_nops_a.gb" &&
            entry.path().filename() != "hblank_int_scx2_nops_b.gb" &&
            entry.path().filename() != "hblank_int_scx4.gb" &&
            entry.path().filename() != "hblank_int_scx5.gb" &&
            entry.path().filename() != "hblank_int_scx5_if_b.gb" &&
            entry.path().filename() != "hblank_int_scx5_if_c.gb" &&
            entry.path().filename() != "hblank_int_scx5_if_d.gb" &&
            entry.path().filename() != "hblank_int_scx5_nops_a.gb" &&
            entry.path().filename() != "hblank_int_scx5_nops_b.gb" &&
            entry.path().filename() != "hblank_int_scx6.gb" &&
            entry.path().filename() != "hblank_int_scx6_if_b.gb" &&
            entry.path().filename() != "hblank_int_scx6_if_c.gb" &&
            entry.path().filename() != "hblank_int_scx6_if_d.gb" &&
            entry.path().filename() != "hblank_int_scx6_nops_a.gb" &&
            entry.path().filename() != "hblank_int_scx6_nops_b.gb" &&
            entry.path().filename() != "hblank_int_scx7.gb" &&
            entry.path().filename() != "hblank_scx2_if_a.gb" &&
            entry.path().filename() != "int_hblank_halt_scx0.gb" &&
            entry.path().filename() != "int_hblank_halt_scx1.gb" &&
            entry.path().filename() != "int_hblank_halt_scx2.gb" &&
            entry.path().filename() != "int_hblank_halt_scx3.gb" &&
            entry.path().filename() != "int_hblank_halt_scx4.gb" &&
            entry.path().filename() != "int_hblank_halt_scx5.gb" &&
            entry.path().filename() != "int_hblank_halt_scx6.gb" &&
            entry.path().filename() != "int_hblank_halt_scx7.gb" &&
            entry.path().filename() != "int_hblank_incs_scx0.gb" &&
            entry.path().filename() != "int_hblank_incs_scx1.gb" &&
            entry.path().filename() != "int_hblank_incs_scx2.gb" &&
            entry.path().filename() != "int_hblank_incs_scx3.gb" &&
            entry.path().filename() != "int_hblank_incs_scx4.gb" &&
            entry.path().filename() != "int_hblank_incs_scx5.gb" &&
            entry.path().filename() != "int_hblank_incs_scx6.gb" &&
            entry.path().filename() != "int_hblank_incs_scx7.gb" &&
            entry.path().filename() != "int_hblank_nops_scx0.gb" &&
            entry.path().filename() != "int_hblank_nops_scx1.gb" &&
            entry.path().filename() != "int_hblank_nops_scx2.gb" &&
            entry.path().filename() != "int_hblank_nops_scx3.gb" &&
            entry.path().filename() != "int_hblank_nops_scx4.gb" &&
            entry.path().filename() != "int_hblank_nops_scx5.gb" &&
            entry.path().filename() != "int_hblank_nops_scx6.gb" &&
            entry.path().filename() != "int_hblank_nops_scx7.gb" &&
            entry.path().filename() != "int_lyc_halt.gb" &&
            entry.path().filename() != "int_lyc_incs.gb" &&
            entry.path().filename() != "int_lyc_nops.gb" &&
            entry.path().filename() != "int_oam_halt.gb" &&
            entry.path().filename() != "int_oam_incs.gb" &&
            entry.path().filename() != "int_oam_nops.gb" &&
            entry.path().filename() != "int_timer_halt.gb" &&
            entry.path().filename() != "int_timer_halt_div_b.gb" &&
            entry.path().filename() != "int_vblank1_halt.gb" &&
            entry.path().filename() != "int_vblank1_incs.gb" &&
            entry.path().filename() != "int_vblank1_nops.gb" &&
            entry.path().filename() != "int_vblank2_halt.gb" &&
            entry.path().filename() != "int_vblank2_incs.gb" &&
            entry.path().filename() != "int_vblank2_nops.gb" &&
            entry.path().filename() != "lcdon_halt_to_vblank_int_b.gb" &&
            entry.path().filename() != "lcdon_nops_to_vblank_int_b.gb" &&
            entry.path().filename() != "lcdon_to_if_oam_a.gb" &&
            entry.path().filename() != "lcdon_to_lyc1_int.gb" &&
            entry.path().filename() != "lcdon_to_lyc2_int.gb" &&
            entry.path().filename() != "lcdon_to_lyc3_int.gb" &&
            entry.path().filename() != "lcdon_to_oam_int_l0.gb" &&
            entry.path().filename() != "lcdon_to_oam_int_l1.gb" &&
            entry.path().filename() != "lcdon_to_oam_int_l2.gb" &&
            entry.path().filename() != "lcdon_to_stat1_d.gb" &&
            entry.path().filename() != "lcdon_write_timing.gb" &&
            entry.path().filename() != "line_144_oam_int_c.gb" &&
            entry.path().filename() != "line_153_lyc0_int_inc_sled.gb" &&
            entry.path().filename() != "line_153_lyc0_stat_timing_c.gb" &&
            entry.path().filename() != "line_153_lyc0_stat_timing_f.gb" &&
            entry.path().filename() != "line_153_lyc153_stat_timing_b.gb" &&
            entry.path().filename() != "line_153_lyc153_stat_timing_e.gb" &&
            entry.path().filename() != "line_153_lyc_b.gb" &&
            entry.path().filename() != "line_153_lyc_int_b.gb" &&
            entry.path().filename() != "ly_while_lcd_off.gb" &&
            entry.path().filename() != "lyc1_int_halt_a.gb" &&
            entry.path().filename() != "lyc1_int_halt_b.gb" &&
            entry.path().filename() != "lyc1_int_nops_a.gb" &&
            entry.path().filename() != "lyc1_int_nops_b.gb" &&
            entry.path().filename() != "lyc1_write_timing_c.gb" &&
            entry.path().filename() != "lyc2_int_halt_a.gb" &&
            entry.path().filename() != "lyc2_int_halt_b.gb" &&
            entry.path().filename() != "lyc_int_halt_a.gb" &&
            entry.path().filename() != "lyc_int_halt_b.gb" &&
            entry.path().filename() != "mbc1_ram_banks.gb" &&
            entry.path().filename() != "mbc1_rom_banks.gb" &&
            entry.path().filename() != "minimal.gb" &&
            entry.path().filename() != "mode2_stat_int_to_oam_unlock.gb" &&
            entry.path().filename() != "oam_int_halt_a.gb" &&
            entry.path().filename() != "oam_int_halt_b.gb" &&
            entry.path().filename() != "oam_int_if_edge_b.gb" &&
            entry.path().filename() != "oam_int_if_edge_d.gb" &&
            entry.path().filename() != "oam_int_if_level_d.gb" &&
            entry.path().filename() != "oam_int_inc_sled.gb" &&
            entry.path().filename() != "oam_int_nops_a.gb" &&
            entry.path().filename() != "oam_int_nops_b.gb" &&
            entry.path().filename() != "oam_sprite_trashing.gb" &&
            entry.path().filename() != "poweron.gb" &&
            entry.path().filename() != "poweron_stat_006.gb" &&
            entry.path().filename() != "ppu_scx_vs_bgp.gb" &&
            entry.path().filename() != "ppu_sprite_testbench.gb" &&
            entry.path().filename() != "ppu_spritex_vs_scx.gb" &&
            entry.path().filename() != "ppu_win_vs_wx.gb" &&
            entry.path().filename() != "ppu_wx_early.gb" &&
            entry.path().filename() != "stat_write_glitch_l0_c.gb" &&
            entry.path().filename() != "stat_write_glitch_l143_c.gb" &&
            entry.path().filename() != "stat_write_glitch_l154_c.gb" &&
            entry.path().filename() != "stat_write_glitch_l154_d.gb" &&
            entry.path().filename() != "stat_write_glitch_l1_d.gb" &&
            entry.path().filename() != "temp.gb" &&
            entry.path().filename() != "toggle_lcdc.gb" &&
            entry.path().filename() != "vblank2_int_halt_b.gb" &&
            entry.path().filename() != "vblank2_int_if_a.gb" &&
            entry.path().filename() != "vblank2_int_if_c.gb" &&
            entry.path().filename() != "vblank2_int_inc_sled.gb" &&
            entry.path().filename() != "vblank2_int_nops_b.gb" &&
            entry.path().filename() != "wave_write_to_0xC003.gb")
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
    GameBoyCore::Emulator game_boy_emulator;
    std::string error_message{};

    void SetUp() override
    {
        ASSERT_TRUE(std::filesystem::exists(GetParam())) << "ROM file not found: " << GetParam();
        game_boy_emulator.try_load_file_to_memory(GetParam(), false, error_message);
        game_boy_emulator.set_post_boot_state();
    }
};

TEST_P(GbmicroTest, TestRom)
{
    const std::filesystem::path test_rom_path = GetParam();
    SCOPED_TRACE("Test ROM: " + test_rom_path.string());

    constexpr size_t max_instructions_before_timeout = 1'000'000;
    bool did_test_succeed = false;

    for (size_t _ = 0; _ < max_instructions_before_timeout; _++)
    {
        game_boy_emulator.step_central_processing_unit_single_instruction();

        const uint8_t test_result_byte = game_boy_emulator.read_byte_from_memory(0xff80);
        const uint8_t test_expected_result_byte = game_boy_emulator.read_byte_from_memory(0xff81);
        const uint8_t test_pass_fail_byte = game_boy_emulator.read_byte_from_memory(0xff82);

        if (test_pass_fail_byte == 0xff)
        {
            FAIL() << "Test failed with result 0x" << std::hex << static_cast<int>(test_result_byte) << ". Expected result was 0x" << static_cast<int>(test_expected_result_byte) << "\n";
        }
        else if (test_pass_fail_byte == 0x01)
        {
            did_test_succeed = true;
            break;
        }
    }
    ASSERT_TRUE(did_test_succeed) << "Test didn't reach a finished state within " << max_instructions_before_timeout << " instructions";
}

INSTANTIATE_TEST_SUITE_P
(
    GbmicroTests,
    GbmicroTest,
    testing::ValuesIn(get_test_rom_paths_in_directory(get_test_directory_path())),
    [](auto info)
    {
        std::string test_rom_file_name = info.param.stem().string();
        std::replace(test_rom_file_name.begin(), test_rom_file_name.end(), '-', '_');
        return test_rom_file_name;
    }
);
