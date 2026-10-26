#include <algorithm>
#include <cctype>
#include <filesystem>
#include <gtest/gtest.h>
#include <string>
#include <vector>

#include "emulator.h"
#include "pixel_processing_unit.h"

static constexpr uint16_t SERIAL_TRANSFER_DATA_ADDRESS = 0xFF01;
static constexpr uint16_t SERIAL_TRANSFER_CONTROL_ADDRESS = 0xFF02;
static constexpr uint16_t TEST_RESULT_ADDRESS = 0xA000;
static constexpr uint16_t TEST_OUTPUT_ADDRESS = 0xA004;

static constexpr uint8_t SERIAL_TRANSFER_REQUEST = 0x81;
static constexpr uint8_t TEST_RUNNING = 0x80;

enum class BlarggResultProtocol
{
    SerialOutput,
    MemorySignature,
    FramebufferChecksum
};

struct BlarggTestCase
{
    std::filesystem::path test_rom_path;
    BlarggResultProtocol result_protocol;
    uint32_t expected_framebuffer_checksum{};
};

static std::filesystem::path get_test_directory_path()
{
    return std::filesystem::path(PROJECT_ROOT) / "tests" / "data" / "blargg-tests" / "gb-test-roms";
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
        if (entry.is_regular_file() && entry.path().extension() == ".gb")
        {
            test_rom_paths.push_back(entry.path());
        }
    }
    std::sort(test_rom_paths.begin(), test_rom_paths.end());
    return test_rom_paths;
}

static void append_test_cases_in_directory(
    std::vector<BlarggTestCase>& test_cases,
    const std::filesystem::path& directory,
    BlarggResultProtocol result_protocol)
{
    for (const auto& test_rom_path : get_test_rom_paths_in_directory(directory))
    {
        test_cases.push_back({ test_rom_path, result_protocol });
    }
}

static std::vector<BlarggTestCase> get_test_cases()
{
    const std::filesystem::path test_directory_path = get_test_directory_path();
    std::vector<BlarggTestCase> test_cases
    {
        { test_directory_path / "halt_bug.gb", BlarggResultProtocol::FramebufferChecksum, 0x1A4E5E56 },
        { test_directory_path / "instr_timing" / "instr_timing.gb", BlarggResultProtocol::SerialOutput }
    };

    append_test_cases_in_directory(test_cases, test_directory_path / "cpu_instrs" / "individual", BlarggResultProtocol::SerialOutput);
    append_test_cases_in_directory(test_cases, test_directory_path / "mem_timing" / "individual", BlarggResultProtocol::SerialOutput);
    append_test_cases_in_directory(test_cases, test_directory_path / "mem_timing-2" / "rom_singles", BlarggResultProtocol::MemorySignature);
    append_test_cases_in_directory(test_cases, test_directory_path / "dmg_sound" / "rom_singles", BlarggResultProtocol::MemorySignature);
    append_test_cases_in_directory(test_cases, test_directory_path / "oam_bug" / "rom_singles", BlarggResultProtocol::MemorySignature);

    return test_cases;
}

static std::string read_memory_test_output(GameBoyEmulator::Emulator& game_boy_emulator)
{
    constexpr size_t MAX_OUTPUT_LENGTH = 1024;
    std::string test_output;

    for (size_t i = 0; i < MAX_OUTPUT_LENGTH; i++)
    {
        const uint8_t character = game_boy_emulator.read_byte_from_memory(static_cast<uint16_t>(TEST_OUTPUT_ADDRESS + i));
        if (character == 0)
        {
            break;
        }
        test_output.push_back(static_cast<char>(character));
    }

    return test_output;
}

static uint32_t get_framebuffer_checksum(GameBoyEmulator::Emulator& game_boy_emulator)
{
    constexpr uint32_t FNV_OFFSET_BASIS = 2'166'136'261;
    constexpr uint32_t FNV_PRIME = 16'777'619;
    constexpr size_t FRAMEBUFFER_SIZE = GameBoyEmulator::DISPLAY_WIDTH_PIXELS * GameBoyEmulator::DISPLAY_HEIGHT_PIXELS;
    const uint8_t frame_buffer_index = game_boy_emulator.get_published_frame_buffer_index_thread_safe();
    const auto& frame_buffer = game_boy_emulator.get_pixel_frame_buffer(frame_buffer_index);
    uint32_t checksum = FNV_OFFSET_BASIS;

    for (size_t i = 0; i < FRAMEBUFFER_SIZE; i++)
    {
        checksum = (checksum ^ frame_buffer[i]) * FNV_PRIME;
    }
    return checksum;
}

class BlarggTest : public testing::TestWithParam<BlarggTestCase>
{
protected:
    GameBoyEmulator::Emulator game_boy_emulator;
    std::string error_message{};

    void SetUp() override
    {
        ASSERT_TRUE(std::filesystem::exists(GetParam().test_rom_path)) << "ROM file not found: " << GetParam().test_rom_path;
        ASSERT_TRUE(game_boy_emulator.try_to_load_file_to_memory(GetParam().test_rom_path, GameBoyEmulator::FileType::GameROM, error_message)) << error_message;
        game_boy_emulator.reset_state();
    }
};

TEST_P(BlarggTest, TestRom)
{
    const std::filesystem::path test_rom_path = GetParam().test_rom_path;
    SCOPED_TRACE("Test ROM: " + test_rom_path.string());

    constexpr size_t MAX_INSTRUCTIONS_BEFORE_TIMEOUT = 50'000'000;
    std::string test_output;
    uint8_t test_result = TEST_RUNNING;
    bool did_test_complete = false;
    uint64_t previous_frame_sequence_number = game_boy_emulator.get_published_frame_sequence_number_thread_safe();
    uint32_t framebuffer_checksum = 0;

    for (size_t _ = 0; _ < MAX_INSTRUCTIONS_BEFORE_TIMEOUT; _++)
    {
        game_boy_emulator.execute_next_instruction();

        if (GetParam().result_protocol == BlarggResultProtocol::SerialOutput)
        {
            if (game_boy_emulator.read_byte_from_memory(SERIAL_TRANSFER_CONTROL_ADDRESS) == SERIAL_TRANSFER_REQUEST)
            {
                test_output.push_back(static_cast<char>(game_boy_emulator.read_byte_from_memory(SERIAL_TRANSFER_DATA_ADDRESS)));
                game_boy_emulator.write_byte_to_memory(SERIAL_TRANSFER_CONTROL_ADDRESS, 0);
            }

            if (test_output.find("Passed") != std::string::npos)
            {
                test_result = 0;
                did_test_complete = true;
                break;
            }
            if (test_output.find("Failed") != std::string::npos)
            {
                test_result = 1;
                did_test_complete = true;
                break;
            }
        }
        else if (GetParam().result_protocol == BlarggResultProtocol::MemorySignature)
        {
            const bool has_valid_signature =
                game_boy_emulator.read_byte_from_memory(TEST_RESULT_ADDRESS + 1) == 0xDE &&
                game_boy_emulator.read_byte_from_memory(TEST_RESULT_ADDRESS + 2) == 0xB0 &&
                game_boy_emulator.read_byte_from_memory(TEST_RESULT_ADDRESS + 3) == 0x61;

            if (has_valid_signature)
            {
                test_result = game_boy_emulator.read_byte_from_memory(TEST_RESULT_ADDRESS);
                if (test_result != TEST_RUNNING)
                {
                    test_output = read_memory_test_output(game_boy_emulator);
                    did_test_complete = true;
                    break;
                }
            }
        }
        else
        {
            const uint64_t frame_sequence_number = game_boy_emulator.get_published_frame_sequence_number_thread_safe();
            if (frame_sequence_number != previous_frame_sequence_number)
            {
                previous_frame_sequence_number = frame_sequence_number;
                framebuffer_checksum = get_framebuffer_checksum(game_boy_emulator);
                if (framebuffer_checksum == GetParam().expected_framebuffer_checksum)
                {
                    test_result = 0;
                    did_test_complete = true;
                    break;
                }
            }
        }
    }

    ASSERT_TRUE(did_test_complete)
        << "Test didn't reach a finished state within " << MAX_INSTRUCTIONS_BEFORE_TIMEOUT << " instructions"
        << ". Output:\n" << test_output
        << ". Last framebuffer checksum: 0x" << std::hex << framebuffer_checksum;
    ASSERT_EQ(test_result, 0) << "Output:\n" << test_output;
}

INSTANTIATE_TEST_SUITE_P(
    BlarggDmgTests,
    BlarggTest,
    testing::ValuesIn(get_test_cases()),
    [](auto info)
    {
        std::filesystem::path test_name_path = info.param.test_rom_path.lexically_relative(get_test_directory_path());
        test_name_path.replace_extension();
        std::string test_name = test_name_path.generic_string();
        std::replace_if(
            test_name.begin(),
            test_name.end(),
            [](unsigned char character) { return !std::isalnum(character); },
            '_');
        return test_name;
    });
