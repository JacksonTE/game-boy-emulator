#include <algorithm>
#include <array>
#include <bit>
#include <format>

#include "console_output_utilities.h"
#include "game_cartridge_slot.h"

namespace GameBoyCore
{

GameCartridgeSlot::GameCartridgeSlot()
{
    reset_state();
}

void GameCartridgeSlot::reset_state()
{
    rom.resize(MemoryBankControllerBase::ROM_ONLY_WITH_NO_MBC_FILE_SIZE, 0);
    ram.resize(0);
    memory_bank_controller = std::make_unique<MemoryBankControllerBase>(rom, ram);
}

bool GameCartridgeSlot::try_load_file(const std::filesystem::path &file_path, std::ifstream &file, std::streamsize file_length_in_bytes, std::string &error_message)
{
    if (file_length_in_bytes < MemoryBankControllerBase::ROM_ONLY_WITH_NO_MBC_FILE_SIZE)
    {
        return set_error_message_and_fail(std::string("Provided file of size ") + std::to_string(file_length_in_bytes) +
                                          std::string(" bytes does not meet the game ROM size requirement."), error_message);
    }

    static constexpr uint8_t expected_logo[LOGO_SIZE] =
    {
        0xCE, 0xED, 0x66, 0x66, 0xCC, 0x0D, 0x00, 0x0B,
        0x03, 0x73, 0x00, 0x83, 0x00, 0x0C, 0x00, 0x0D,
        0x00, 0x08, 0x11, 0x1F, 0x88, 0x89, 0x00, 0x0E,
        0xDC, 0xCC, 0x6E, 0xE6, 0xDD, 0xDD, 0xD9, 0x99,
        0xBB, 0xBB, 0x67, 0x63, 0x6E, 0x0E, 0xEC, 0xCC,
        0xDD, 0xDC, 0x99, 0x9F, 0xBB, 0xB9, 0x33, 0x3E
    };
    std::array<uint8_t, LOGO_SIZE> logo_bytes{};
    file.seekg(LOGO_START_POSITION, std::ios::beg);
    file.read(reinterpret_cast<char *>(logo_bytes.data()), logo_bytes.size());

    if (!std::equal(logo_bytes.begin(), logo_bytes.end(), std::begin(expected_logo)))
    {
        return set_error_message_and_fail(std::string("Logo in provided ROM does not match the expected pattern."), error_message);
    }

    uint8_t color_game_boy_required_flag = 0xC0;
    file.seekg(0x143, std::ios::beg);
    file.read(reinterpret_cast<char *>(&color_game_boy_required_flag), 1);

    if (color_game_boy_required_flag == 0xC0)
    {
        return set_error_message_and_fail(std::string("Provided game ROM requires Game Boy Color functionality to run."), error_message);
    }

    uint8_t cartridge_type = 0x00;
    file.seekg(0x147, std::ios::beg);
    file.read(reinterpret_cast<char *>(&cartridge_type), 1);

    uint8_t cartridge_rom_size_byte = 0x00;
    file.seekg(0x148, std::ios::beg);
    file.read(reinterpret_cast<char *>(&cartridge_rom_size_byte), 1);
    if (cartridge_rom_size_byte > 0x08)
    {
        return set_error_message_and_fail(std::string("Provided game ROM contains an invalid ROM size byte."), error_message);
    }
    const uint32_t expected_cartridge_rom_size = 0x8000 * (1 << cartridge_rom_size_byte);
    if (file_length_in_bytes != expected_cartridge_rom_size)
    {
        return set_error_message_and_fail(std::string("Provided file's size does not match the size specified in its header."), error_message);
    }

    uint8_t cartridge_ram_size_byte = 0x00;
    file.seekg(0x149, std::ios::beg);
    file.read(reinterpret_cast<char *>(&cartridge_ram_size_byte), 1);
    if (cartridge_ram_size_byte == 0x01 || cartridge_ram_size_byte > 0x05)
    {
        return set_error_message_and_fail(std::string("Provided game ROM contains an invalid RAM size byte."), error_message);
    }
    uint32_t cartridge_ram_size = 0;
    switch (cartridge_ram_size_byte)
    {
        case 0x02:
            cartridge_ram_size = 0x2000;
            break;
        case 0x03:
            cartridge_ram_size = 0x8000;
            break;
        case 0x04:
            cartridge_ram_size = 0x20000;
            break;
        case 0x05:
            cartridge_ram_size = 0x10000;
            break;
    }

    file.seekg(0, std::ios::beg);
    bool was_file_load_successful = false;

    switch (cartridge_type)
    {
        case ROM_ONLY_BYTE:
        {
            if (file_length_in_bytes > MemoryBankControllerBase::ROM_ONLY_WITH_NO_MBC_FILE_SIZE)
            {
                return set_error_message_and_fail(std::string("Provided file does not meet the size requirement for a ROM-only game."), error_message);
            }
            if (cartridge_ram_size != 0)
            {
                return set_error_message_and_fail(std::string("Provided game ROM contains an invalid RAM size byte for a ROM-only game."), error_message);
            }
            rom.resize(MemoryBankControllerBase::ROM_ONLY_WITH_NO_MBC_FILE_SIZE, 0);
            ram.resize(0);
            memory_bank_controller = std::make_unique<MemoryBankControllerBase>(rom, ram);
            was_file_load_successful = static_cast<bool>(file.read(reinterpret_cast<char *>(rom.data()), file_length_in_bytes));
            break;
        }
        case MBC1_BYTE:
        case MBC1_WITH_RAM_BYTE:
        case MBC1_WITH_RAM_AND_BATTERY_BYTE:
        {
            if (file_length_in_bytes > MBC1::MAX_ROM_SIZE)
            {
                return set_error_message_and_fail(std::string("Provided file does not meet the size requirement for an MBC1 game."), error_message);
            }
            if ((cartridge_type == MBC1_BYTE && cartridge_ram_size != 0) ||
                (file_length_in_bytes > MBC1::MAX_ROM_SIZE_IN_DEFAULT_CONFIGURATION && cartridge_ram_size > MBC1::MAX_RAM_SIZE_IN_LARGE_CONFIGURATION))
            {
                return set_error_message_and_fail(std::string("Provided game ROM contains an invalid RAM size byte for its selected memory bank controller."), error_message);
            }
            rom.resize(std::bit_ceil(static_cast<uint32_t>(file_length_in_bytes)), 0);
            ram.resize(cartridge_ram_size);
            was_file_load_successful = static_cast<bool>(file.read(reinterpret_cast<char *>(rom.data()), file_length_in_bytes));

            if (was_file_load_successful && rom.size() == MBC1::MBC1M_MULTI_GAME_COMPILATION_CART_ROM_SIZE)
            {
                const uint32_t rom_bank_0x10_offset = 0x10 * ROM_BANK_SIZE;
                const bool is_mbc1m_cartridge = std::equal(rom.begin() + rom_bank_0x10_offset + LOGO_START_POSITION, 
                                                           rom.begin() + rom_bank_0x10_offset + LOGO_START_POSITION + LOGO_SIZE,
                                                           std::begin(expected_logo));
                if (is_mbc1m_cartridge)
                {
                    // Convert into standard MBC1 cartridge so normal indexing can be used to read
                    rom.resize(rom.size() << 1);
                    const uint8_t memory_banks_per_sub_rom = 0x10;

                    // ROM contains 4 sub-ROMs taking up 25% of the ROM space
                    // To convert to normal MBC1-readable format, each sub-ROM needs to be be duplicated once with subsequent sub-ROMs shifted and duplicated accordingly
                    for (int8_t sub_rom_number = 3; sub_rom_number >= 0; sub_rom_number--)
                    {
                        for (int8_t memory_bank_number = memory_banks_per_sub_rom - 1; memory_bank_number >= 0; memory_bank_number--)
                        {
                            uint32_t previous_global_memory_bank_number = (sub_rom_number * memory_banks_per_sub_rom) + memory_bank_number;
                            uint32_t previous_global_memory_bank_offset = static_cast<uint32_t>(previous_global_memory_bank_number) * ROM_BANK_SIZE;

                            uint32_t new_global_memory_bank_number_for_first_copy = ((2 * sub_rom_number) * memory_banks_per_sub_rom) + memory_bank_number;
                            uint32_t new_global_memory_bank_number_for_second_copy = ((2 * sub_rom_number + 1) * memory_banks_per_sub_rom) + memory_bank_number;

                            uint32_t new_global_memory_bank_offset_for_first_copy = static_cast<uint32_t>(new_global_memory_bank_number_for_first_copy) * ROM_BANK_SIZE;
                            uint32_t new_global_memory_bank_offset_for_second_copy = static_cast<uint32_t>(new_global_memory_bank_number_for_second_copy) * ROM_BANK_SIZE;

                            std::copy(rom.begin() + previous_global_memory_bank_offset,
                                      rom.begin() + previous_global_memory_bank_offset + ROM_BANK_SIZE,
                                      rom.begin() + new_global_memory_bank_offset_for_first_copy);
                            std::copy(rom.begin() + previous_global_memory_bank_offset,
                                      rom.begin() + previous_global_memory_bank_offset + ROM_BANK_SIZE,
                                      rom.begin() + new_global_memory_bank_offset_for_second_copy);
                        }
                    }
                }
            }
            memory_bank_controller = std::make_unique<MBC1>(rom, ram);
            break;
        }
        case MBC2_BYTE:
        case MBC2_WITH_BATTERY_BYTE:
        {
            if (file_length_in_bytes > MBC2::MAX_ROM_SIZE)
            {
                return set_error_message_and_fail(std::string("Provided file does not meet the size requirement for an MBC2 game."), error_message);
            }
            if (cartridge_ram_size != 0)
            {
                return set_error_message_and_fail(std::string("Provided game ROM contains an invalid RAM size byte for its selected memory bank controller."), error_message);
            }
            rom.resize(std::bit_ceil(static_cast<uint32_t>(file_length_in_bytes)), 0);
            ram.resize(MBC2::BUILT_IN_RAM_SIZE, 0xF0);
            was_file_load_successful = static_cast<bool>(file.read(reinterpret_cast<char *>(rom.data()), file_length_in_bytes));
            memory_bank_controller = std::make_unique<MBC2>(rom, ram);
            break;
        }
        case MBC3_WITH_TIMER_AND_BATTERY_BYTE:
        case MBC3_WITH_TIMER_AND_RAM_AND_BATTERY_BYTE:
        case MBC3_BYTE:
        case MBC3_WITH_RAM_BYTE:
        case MBC3_WITH_RAM_AND_BATTERY_BYTE:
        {
            if (file_length_in_bytes > MBC3::MAX_ROM_SIZE)
            {
                return set_error_message_and_fail(std::string("Provided file does not meet the size requirement for an MBC3 game."), error_message);
            }
            if (((cartridge_type == MBC3_BYTE || cartridge_ram_size == MBC3_WITH_TIMER_AND_BATTERY_BYTE) && cartridge_ram_size != 0) ||
                cartridge_ram_size > MBC3::MAX_RAM_SIZE)
            {
                return set_error_message_and_fail(std::string("Provided game ROM contains an invalid RAM size byte for its selected memory bank controller."), error_message);
            }
            rom.resize(std::bit_ceil(static_cast<uint32_t>(file_length_in_bytes)), 0);
            ram.resize(cartridge_ram_size);
            was_file_load_successful = static_cast<bool>(file.read(reinterpret_cast<char *>(rom.data()), file_length_in_bytes));
            memory_bank_controller = std::make_unique<MBC3>(rom, ram);
            break;
        }
        case MBC5_BYTE:
        case MBC5_WITH_RAM_BYTE:
        case MBC5_WITH_RAM_AND_BATTERY_BYTE:
        case MBC5_WITH_RUMBLE_AND_RAM:
        case MBC5_WITH_RUMBLE_AND_RAM_AND_BATTERY:
        {
            if (file_length_in_bytes > MBC5::MAX_ROM_SIZE)
            {
                return set_error_message_and_fail(std::string("Provided file does not meet the size requirement for an MBC5 game."), error_message);
            }
            if ((cartridge_type == MBC5_BYTE && cartridge_ram_size != 0) ||
                cartridge_ram_size > MBC5::MAX_RAM_SIZE)
            {
                return set_error_message_and_fail(std::string("Provided game ROM contains an invalid RAM size byte for its selected memory bank controller."), error_message);
            }
            rom.resize(std::bit_ceil(static_cast<uint32_t>(file_length_in_bytes)), 0);
            ram.resize(cartridge_ram_size);
            was_file_load_successful = static_cast<bool>(file.read(reinterpret_cast<char *>(rom.data()), file_length_in_bytes));
            memory_bank_controller = std::make_unique<MBC5>(rom, ram);
            break;
        }
        default:
        {
            return set_error_message_and_fail(std::format("Game ROM with cartridge type 0x{:02x} is not currently supported.", static_cast<int>(cartridge_type)), error_message);
        }
    }

    if (!was_file_load_successful)
    {
        return set_error_message_and_fail(std::string("Could not read game rom file ") + file_path.string(), error_message);
    }
    return true;
}

uint8_t GameCartridgeSlot::read_byte(uint16_t address) const
{
    return memory_bank_controller->read_byte(address);
}

void GameCartridgeSlot::write_byte(uint16_t address, uint8_t value)
{
    memory_bank_controller->write_byte(address, value);
}

} // namespace GameBoyCore
