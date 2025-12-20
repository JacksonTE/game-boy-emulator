#pragma once

#include <cstdint>
#include <filesystem>
#include <fstream>
#include <memory>

#include "memory_bank_controllers.h"

namespace GameBoyEmulator
{

constexpr uint8_t LOGO_SIZE = 48;
constexpr uint16_t LOGO_START_POSITION = 0x0104;

constexpr uint16_t GAME_ROM_TITLE_START = 0x0134;
constexpr uint8_t GAME_ROM_TITLE_MAX_LENGTH = 16;
constexpr uint8_t GAME_ROM_TITLE_FINISHED_BYTE = 0x00;

class GameCartridgeSlot
{
public:
    GameCartridgeSlot();

    void reset_state();

    bool try_load_file(
        const std::filesystem::path& file_path,
        std::ifstream& file,
        std::streamsize file_length_in_bytes,
        std::string& error_message);

    uint8_t read_byte(uint16_t address) const;
    void write_byte(uint16_t address, uint8_t value);

    bool has_battery_backed_ram() const;
    const std::string& get_game_title() const;

    bool try_load_ram_from_save_file(const std::filesystem::path& save_directory);
    bool try_save_ram_to_save_file(const std::filesystem::path& save_directory) const;

private:
    std::string get_sanitized_save_filename() const;

    std::vector<uint8_t> rom{};
    std::vector<uint8_t> ram{};
    std::unique_ptr<MemoryBankControllerBase> memory_bank_controller{};

    std::string game_rom_title{};
    bool has_battery{};
};

} // namespace GameBoyEmulator
