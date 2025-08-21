#pragma once

#include <cstdint>
#include <filesystem>
#include <fstream>
#include <memory>

#include "memory_bank_controllers.h"

namespace GameBoyCore
{

constexpr uint8_t LOGO_SIZE = 48;
constexpr uint16_t LOGO_START_POSITION = 0x0104;


class GameCartridgeSlot
{
public:
    GameCartridgeSlot();

    void reset_state();

    bool try_load_file(const std::filesystem::path& file_path, std::ifstream& file, std::streamsize file_length_in_bytes, std::string& error_message);

    uint8_t read_byte(uint16_t address) const;
    void write_byte(uint16_t address, uint8_t value);

private:
    std::vector<uint8_t> rom{};
    std::vector<uint8_t> ram{};
    std::unique_ptr<MemoryBankControllerBase> memory_bank_controller{};
};

} // namespace GameBoyCore
