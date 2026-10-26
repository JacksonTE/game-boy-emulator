#include <algorithm>
#include <filesystem>
#include <gtest/gtest.h>
#include <string>
#include <unordered_set>
#include <vector>

#include "emulator.h"
#include "pixel_processing_unit.h"

static std::filesystem::path get_test_directory_path()
{
    return std::filesystem::path(PROJECT_ROOT) / "tests" / "data" / "gbmicrotest" / "bin";
}

static const std::unordered_set<std::filesystem::path> disabled_test_rom_filenames
{
    "000-oam_lock.gb",
    "000-write_to_x8000.gb",
    "001-vram_unlocked.gb",
    "002-vram_locked.gb",
    "004-tima_boot_phase.gb",
    "004-tima_cycle_timer.gb",
    "007-lcd_on_stat.gb",
    "400-dma.gb",
    "500-scx-timing.gb",
    "800-ppu-latch-scx.gb",
    "801-ppu-latch-scy.gb",
    "802-ppu-latch-tileselect.gb",
    "803-ppu-latch-bgdisplay.gb",
    "audio_testbench.gb",
    "cpu_bus_1.gb",
    "dma_basic.gb",
    "flood_vram.gb",
    "halt_op_dupe_delay.gb",
    "lcdon_write_timing.gb",
    "line_153_lyc_b.gb",
    "ly_while_lcd_off.gb",
    "mbc1_rom_banks.gb",
    "minimal.gb",
    "mode2_stat_int_to_oam_unlock.gb",
    "oam_sprite_trashing.gb",
    "poweron.gb",
    "ppu_scx_vs_bgp.gb",
    "ppu_sprite_testbench.gb",
    "ppu_spritex_vs_scx.gb",
    "ppu_win_vs_wx.gb",
    "ppu_wx_early.gb",
    "stat_write_glitch_l154_d.gb",
    "temp.gb",
    "toggle_lcdc.gb",
    "wave_write_to_0xC003.gb"
};

static std::vector<std::filesystem::path> get_test_rom_paths_in_directory(const std::filesystem::path& directory)
{
    if (!std::filesystem::exists(directory))
    {
        return { directory };
    }
    std::vector<std::filesystem::path> test_rom_paths;

    for (const auto& entry : std::filesystem::directory_iterator(directory))
    {
        if (entry.is_regular_file() &&
            entry.path().extension() == ".gb" &&
            !disabled_test_rom_filenames.contains(entry.path().filename()))
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

struct GbmicroImageComparisonTestCase
{
    std::string rom_filename;
    uint8_t (*get_expected_pixel)(size_t x, size_t y);
};

static bool is_box_pixel(size_t x, size_t y)
{
    return y % 8 >= 1 && y % 8 <= 6 && x % 8 >= 1 && x % 8 <= 6;
}

static uint8_t get_expected_scx_latch_pixel(size_t x, size_t y)
{
    return is_box_pixel(x + 4, y) ? 3 : 0;
}

static uint8_t get_expected_scy_latch_pixel(size_t x, size_t y)
{
    const size_t scy = x < 8 ? 4 : 0;
    return is_box_pixel(x, y + scy) ? 3 : 0;
}

static uint8_t get_expected_tile_select_latch_pixel(size_t x, size_t y)
{
    return x < 32 && is_box_pixel(x, y) ? 3 : 0;
}

static uint8_t get_expected_background_display_latch_pixel(size_t x, size_t y)
{
    return x >= 1 && is_box_pixel(x, y) ? 3 : 0;
}

class GbmicroImageComparisonTest : public testing::TestWithParam<GbmicroImageComparisonTestCase>
{
protected:
    GameBoyEmulator::Emulator game_boy_emulator;
    std::string error_message{};

    void SetUp() override
    {
        const std::filesystem::path test_rom_path = get_test_directory_path() / GetParam().rom_filename;
        ASSERT_TRUE(std::filesystem::exists(test_rom_path)) << "ROM file not found: " << test_rom_path;
        ASSERT_TRUE(game_boy_emulator.try_to_load_file_to_memory(test_rom_path, GameBoyEmulator::FileType::GameROM, error_message)) << error_message;
        game_boy_emulator.reset_state();
    }
};

TEST_P(GbmicroImageComparisonTest, TestRomPicture)
{
    SCOPED_TRACE("Test ROM: " + GetParam().rom_filename);

    constexpr size_t MAX_INSTRUCTIONS_BEFORE_TIMEOUT = 1'000'000;
    constexpr uint64_t FRAMES_BEFORE_TEST_RESULT = 2;
    const uint64_t expected_frame_sequence_number = game_boy_emulator.get_published_frame_sequence_number_thread_safe() + FRAMES_BEFORE_TEST_RESULT;
    bool did_test_produce_frame = false;

    for (size_t _ = 0; _ < MAX_INSTRUCTIONS_BEFORE_TIMEOUT; _++)
    {
        game_boy_emulator.execute_next_instruction();

        if (game_boy_emulator.get_published_frame_sequence_number_thread_safe() >= expected_frame_sequence_number)
        {
            did_test_produce_frame = true;
            break;
        }
    }
    ASSERT_TRUE(did_test_produce_frame) << "Test didn't produce the expected frame within " << MAX_INSTRUCTIONS_BEFORE_TIMEOUT << " instructions";

    const uint8_t frame_buffer_index = game_boy_emulator.get_published_frame_buffer_index_thread_safe();
    const auto& frame_buffer = game_boy_emulator.get_pixel_frame_buffer(frame_buffer_index);
    size_t mismatched_pixel_count = 0;
    size_t first_mismatched_pixel = 0;
    uint8_t first_actual_pixel = 0;
    uint8_t first_expected_pixel = 0;

    for (size_t y = 0; y < GameBoyEmulator::DISPLAY_HEIGHT_PIXELS; y++)
    {
        for (size_t x = 0; x < GameBoyEmulator::DISPLAY_WIDTH_PIXELS; x++)
        {
            const size_t pixel_index = y * GameBoyEmulator::DISPLAY_WIDTH_PIXELS + x;
            const uint8_t expected_pixel = GetParam().get_expected_pixel(x, y);

            if (frame_buffer[pixel_index] != expected_pixel)
            {
                if (mismatched_pixel_count == 0)
                {
                    first_mismatched_pixel = pixel_index;
                    first_actual_pixel = frame_buffer[pixel_index];
                    first_expected_pixel = expected_pixel;
                }
                mismatched_pixel_count++;
            }
        }
    }

    ASSERT_EQ(mismatched_pixel_count, 0)
        << "First mismatched pixel was at ("
        << first_mismatched_pixel % GameBoyEmulator::DISPLAY_WIDTH_PIXELS << ", "
        << first_mismatched_pixel / GameBoyEmulator::DISPLAY_WIDTH_PIXELS << ")"
        << ". Actual: " << static_cast<int>(first_actual_pixel)
        << ". Expected: " << static_cast<int>(first_expected_pixel);
}

INSTANTIATE_TEST_SUITE_P(
    GbmicroImageComparisonTests,
    GbmicroImageComparisonTest,
    testing::Values(
        GbmicroImageComparisonTestCase{ "800-ppu-latch-scx.gb", get_expected_scx_latch_pixel },
        GbmicroImageComparisonTestCase{ "801-ppu-latch-scy.gb", get_expected_scy_latch_pixel },
        GbmicroImageComparisonTestCase{ "802-ppu-latch-tileselect.gb", get_expected_tile_select_latch_pixel },
        GbmicroImageComparisonTestCase{ "803-ppu-latch-bgdisplay.gb", get_expected_background_display_latch_pixel }),
    [](auto info)
    {
        std::string test_rom_file_name = std::filesystem::path(info.param.rom_filename).stem().string();
        std::replace(test_rom_file_name.begin(), test_rom_file_name.end(), '-', '_');
        return test_rom_file_name;
    });
