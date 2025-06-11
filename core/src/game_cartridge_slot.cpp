#include <array>
#include <algorithm>
#include <bit>
#include <cmath>
#include <bit>
#include <iostream>
#include <iomanip>

#include "bitwise_utilities.h"
#include "game_cartridge_slot.h"

namespace GameBoyCore
{

MemoryBankControllerBase::MemoryBankControllerBase(std::vector<uint8_t> &rom, std::vector<uint8_t> &ram)
    : cartridge_rom{rom},
      cartridge_ram{ram}
{
}

uint8_t	MemoryBankControllerBase::read_byte(uint16_t address)
{
    if (address >= MemoryBankControllerBase::no_mbc_file_size_bytes)
    {
        std::cout << std::hex << std::setfill('0')
                  << "Attemped to read from non-existent cartridge RAM at address " << std::setw(4) << address << "Returning 0xff as a fallback.";
        return 0xff;
    }
    return cartridge_rom[address];
}

void MemoryBankControllerBase::write_byte(uint16_t address, uint8_t value)
{
    std::cout << std::hex << std::setfill('0')
              << "Attempted to write to read only address 0x" << std::setw(4) << address << " in a ROM-only cartridge. No operation will occur.\n";
}

MBC1::MBC1(std::vector<uint8_t> &rom, std::vector<uint8_t> &ram)
    : MemoryBankControllerBase{rom, ram}
{
}

uint8_t MBC1::read_byte(uint16_t address)
{
    if (address < 0x4000)
    {
        const bool does_rom_exceed_default_configuration_max = cartridge_rom.size() > MBC1::max_rom_size_in_default_configuration_bytes;
        const uint8_t effective_rom_banking_mode_select = does_rom_exceed_default_configuration_max ? banking_mode_select : 0;
        const uint8_t effective_upper_rom_bank_number = does_rom_exceed_default_configuration_max ? ram_bank_or_upper_rom_bank_number : 0;

        uint32_t address_to_read = (effective_rom_banking_mode_select == 1)
            ? address | (effective_upper_rom_bank_number << 19)
            : address;
        address_to_read &= (cartridge_rom.size() - 1);
        return cartridge_rom[address_to_read];
    }
    else if (address < 0x8000)
    {
        const bool does_rom_exceed_default_configuration_max = cartridge_rom.size() > MBC1::max_rom_size_in_default_configuration_bytes;
        const uint8_t effective_upper_rom_bank_number = does_rom_exceed_default_configuration_max ? ram_bank_or_upper_rom_bank_number : 0;

        uint32_t address_to_read = (address & 0x3fff) |
                                   (effective_rom_bank_number << 14) |
                                   (effective_upper_rom_bank_number << 19);
        address_to_read &= (cartridge_rom.size() - 1);
        return cartridge_rom[address_to_read];
    }
    else if (address >= 0xa000 && address < 0xc000)
    {
        if (!is_ram_enabled)
        {
            return 0xff;
        }
        const bool does_ram_exceed_large_configuration_max = cartridge_ram.size() > MBC1::max_ram_size_in_large_configuration_bytes;

        const uint8_t effective_ram_bank_number = does_ram_exceed_large_configuration_max ? ram_bank_or_upper_rom_bank_number : 0;
        const uint8_t effective_ram_banking_mode_select = does_ram_exceed_large_configuration_max ? banking_mode_select : 0;

        const uint32_t address_to_read = (effective_ram_banking_mode_select == 1)
            ? (address & 0x1fff) | (effective_ram_bank_number << 13)
            : (address & 0x1fff);
        return cartridge_ram[address_to_read];
    }
    throw std::runtime_error("Attemped to read from out of bounds address " + std::to_string(address) + " in the cartridge's ROM or RAM. Exiting.");
}

void MBC1::write_byte(uint16_t address, uint8_t value)
{
    if (address < 0x2000)
    {
        is_ram_enabled = ((value & 0x0f) == 0x0a);
    }
    else if (address < 0x4000)
    {
        effective_rom_bank_number = std::max((value & 0b00011111), 1);
    }
    else if (address < 0x6000)
    {
        ram_bank_or_upper_rom_bank_number = value & 0b00000011;
    }
    else if (address < 0x8000)
    {
        banking_mode_select = value & 0b00000001;
    }
    else if (address >= 0xa000 && address < 0xc000)
    {
        if (!is_ram_enabled)
        {
            return;
        }
        const bool does_ram_exceed_large_configuration_max = cartridge_ram.size() > MBC1::max_ram_size_in_large_configuration_bytes;

        const uint8_t effective_ram_bank_number = does_ram_exceed_large_configuration_max ? ram_bank_or_upper_rom_bank_number : 0;
        const uint8_t effective_ram_banking_mode_select = does_ram_exceed_large_configuration_max ? banking_mode_select : 0;

        const uint32_t address_to_write = (effective_ram_banking_mode_select == 1)
            ? (address & 0x1fff) | (effective_ram_bank_number << 13)
            : (address & 0x1fff);
        cartridge_ram[address_to_write] = value;
    }
    else
        throw std::runtime_error("Attemped to write to an out of bounds address in the cartridge's ROM or RAM. Exiting.");
}

MBC2::MBC2(std::vector<uint8_t> &rom, std::vector<uint8_t> &ram)
    : MemoryBankControllerBase{rom, ram}
{
}

uint8_t MBC2::read_byte(uint16_t address)
{
    if (address < 0x4000)
    {
        return cartridge_rom[address];
    }
    else if (address < 0x8000)
    {
        const uint8_t effective_rom_bank_number = does_register_control_rom ? ram_enable_or_rom_bank_number : 1;
        const uint32_t address_to_read = ((address & 0x3fff) | (effective_rom_bank_number << 14)) & static_cast<uint32_t>(cartridge_rom.size() - 1);
        return cartridge_rom[address_to_read];
    }
    else if (address >= 0xa000 && address < 0xc000)
    {
        const bool is_ram_enabled = !does_register_control_rom && (ram_enable_or_rom_bank_number & 0x0f) == 0x0a;
        if (!is_ram_enabled)
        {
            return 0xff;
        }
        const uint16_t address_to_read = address & (MBC2::built_in_ram_size_bytes - 1);
        return cartridge_ram[address_to_read] | 0xf0;
    }
    throw std::runtime_error("Attemped to read from out of bounds address " + std::to_string(address) + " in the cartridge's ROM or RAM. Exiting.");
}

void MBC2::write_byte(uint16_t address, uint8_t value)
{
    if (address < 0x4000)
    {
        does_register_control_rom = is_bit_set(address, static_cast<uint16_t>(8));
        ram_enable_or_rom_bank_number = does_register_control_rom
            ? std::max(value & (max_number_of_rom_banks - 1), 1)
            : value;
    }
    else if (address < 0x8000)
    {
        std::cout << "Attemped to write to out of bounds address " + std::to_string(address) + " in the cartridge's ROM. No operation will occur.\n";
    }
    else if (address >= 0xa000 && address < 0xc000)
    {
        const bool is_ram_enabled = !does_register_control_rom && (ram_enable_or_rom_bank_number & 0x0f) == 0x0a;
        if (!is_ram_enabled)
        {
            return;
        }
        const uint16_t address_to_write = address & (MBC2::built_in_ram_size_bytes - 1);
        cartridge_ram[address_to_write] = value | 0xf0;
    }
    else
        throw std::runtime_error("Attemped to write to out of bounds address " + std::to_string(address) + " in the cartridge's ROM or RAM. Exiting.");
}

MBC5::MBC5(std::vector<uint8_t> &rom, std::vector<uint8_t> &ram)
    : MemoryBankControllerBase{rom, ram}
{
}

uint8_t MBC5::read_byte(uint16_t address)
{
    if (address < 0x4000)
    {
        return cartridge_rom[address];
    }
    else if (address < 0x8000)
    {
        const uint32_t rom_bank_starting_address = rom_bank_number << std::countr_zero(MBC5::rom_bank_size_bytes);
        const uint16_t address_within_rom_bank = address & (MBC5::rom_bank_size_bytes - 1);
        const uint32_t address_to_read = (rom_bank_starting_address | address_within_rom_bank) & static_cast<uint32_t>(cartridge_rom.size() - 1);
        return cartridge_rom[address_to_read];
    }
    else if (address >= 0xa000 && address < 0xc000)
    {
        if (!is_ram_enabled)
        {
            return 0xff;
        }
        const uint32_t ram_bank_starting_address = ram_bank_number << std::countr_zero(MBC5::ram_bank_size_bytes);
        const uint16_t address_within_ram_bank = address & (MBC5::ram_bank_size_bytes - 1);
        const uint32_t address_to_read = (ram_bank_starting_address | address_within_ram_bank) & static_cast<uint32_t>(cartridge_ram.size() - 1);
        return cartridge_ram[address_to_read];
    }
    throw std::runtime_error("Attemped to read from out of bounds address " + std::to_string(address) + " in the cartridge's ROM or RAM. Exiting.");
}

void MBC5::write_byte(uint16_t address, uint8_t value)
{
    if (address < 0x2000)
    {
        is_ram_enabled = ((value & 0x0f) == 0x0a);
    }
    else if (address < 0x3000)
    {
        const uint8_t bits_0_to_7_of_rom_bank_number = value;
        rom_bank_number = (rom_bank_number & 0xff00) | bits_0_to_7_of_rom_bank_number;
    }
    else if (address < 0x4000)
    {
        const uint16_t bit_8_of_rom_bank_number = (value & 1) << 8;
        rom_bank_number = (rom_bank_number & 0x00ff) | bit_8_of_rom_bank_number;
    }
    else if (address < 0x6000)
    {
        ram_bank_number = value;
    }
    else if (address < 0x8000)
    {
        std::cout << "Attemped to write to out of bounds address " + std::to_string(address) + " in the cartridge's ROM. No operation will occur.\n";
    }
    else if (address >= 0xa000 && address < 0xc000)
    {
        if (!is_ram_enabled)
        {
            return;
        }
        const uint32_t ram_bank_starting_address = ram_bank_number << std::countr_zero(MBC5::ram_bank_size_bytes);
        const uint16_t address_within_ram_bank = address & (MBC5::ram_bank_size_bytes - 1);
        const uint32_t address_to_write = (ram_bank_starting_address | address_within_ram_bank) & static_cast<uint32_t>(cartridge_ram.size() - 1);
        cartridge_ram[address_to_write] = value;
    }
    else
        throw std::runtime_error("Attemped to write to out of bounds address " + std::to_string(address) + " in the cartridge's ROM or RAM. Exiting.");
}

GameCartridgeSlot::GameCartridgeSlot()
{
    reset_state();
}

void GameCartridgeSlot::reset_state()
{
    rom.resize(MemoryBankControllerBase::no_mbc_file_size_bytes, 0);
    ram.resize(0);
    memory_bank_controller = std::make_unique<MemoryBankControllerBase>(rom, ram);
}

bool GameCartridgeSlot::try_load_file(const std::filesystem::path &file_path, std::ifstream &file, std::streamsize file_length_in_bytes, std::string &error_message)
{
    if (file_length_in_bytes < MemoryBankControllerBase::no_mbc_file_size_bytes)
    {
        std::cerr << "Error: Provided file of size " << file_length_in_bytes << " bytes does not meet the game ROM size requirement.\n";
        error_message = "Provided file of size " + std::to_string(file_length_in_bytes) + " bytes does not meet the game ROM size requirement.";
        return false;
    }

    static constexpr uint8_t expected_logo[LOGO_SIZE] =
    {
        0xce, 0xed, 0x66, 0x66, 0xcc, 0x0d, 0x00, 0x0b,
        0x03, 0x73, 0x00, 0x83, 0x00, 0x0c, 0x00, 0x0d,
        0x00, 0x08, 0x11, 0x1f, 0x88, 0x89, 0x00, 0x0e,
        0xdc, 0xcc, 0x6e, 0xe6, 0xdd, 0xdd, 0xd9, 0x99,
        0xbb, 0xbb, 0x67, 0x63, 0x6e, 0x0e, 0xec, 0xcc,
        0xdd, 0xdc, 0x99, 0x9f, 0xbb, 0xb9, 0x33, 0x3e
    };
    std::array<uint8_t, LOGO_SIZE> logo_bytes{};
    file.seekg(LOGO_START_POSITION, std::ios::beg);
    file.read(reinterpret_cast<char *>(logo_bytes.data()), logo_bytes.size());

    if (!std::equal(logo_bytes.begin(), logo_bytes.end(), std::begin(expected_logo)))
    {
        std::cerr << "Error: Logo in provided ROM does not match the expected pattern.\n";
        error_message = "Logo in provided ROM does not match the expected pattern.";
        return false;
    }

    uint8_t color_game_boy_required_flag = 0xc0;
    file.seekg(0x143, std::ios::beg);
    file.read(reinterpret_cast<char *>(&color_game_boy_required_flag), 1);

    if (color_game_boy_required_flag == 0xc0)
    {
        std::cerr << "Error: Provided game ROM requires Game Boy Color functionality to run.\n";
        error_message = "Provided game ROM requires Game Boy Color functionality to run.";
        return false;
    }

    uint8_t cartridge_type = 0x00;
    file.seekg(0x147, std::ios::beg);
    file.read(reinterpret_cast<char *>(&cartridge_type), 1);

    uint8_t cartridge_rom_size_byte = 0x00;
    file.seekg(0x148, std::ios::beg);
    file.read(reinterpret_cast<char *>(&cartridge_rom_size_byte), 1);
    if (cartridge_rom_size_byte > 0x08)
    {
        std::cerr << "Error: Provided game ROM contains an invalid ROM size byte.\n";
        error_message = "Provided game ROM contains an invalid ROM size byte.";
        return false;
    }
    const uint32_t expected_cartridge_rom_size = 0x8000 * (1 << cartridge_rom_size_byte);
    if (file_length_in_bytes != expected_cartridge_rom_size)
    {
        std::cerr << "Error: Provided file's size does not match the size specified in its header.\n";
        error_message = "Provided file's size does not match the size specified in its header.";
        return false;
    }

    uint8_t cartridge_ram_size_byte = 0x00;
    file.seekg(0x149, std::ios::beg);
    file.read(reinterpret_cast<char *>(&cartridge_ram_size_byte), 1);
    if (cartridge_ram_size_byte == 0x01 || cartridge_ram_size_byte > 0x05)
    {
        std::cerr << "Error: Provided game ROM contains an invalid RAM size byte.\n";
        error_message = "Provided game ROM contains an invalid RAM size byte.";
        return false;
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
            if (file_length_in_bytes != MemoryBankControllerBase::no_mbc_file_size_bytes)
            {
                std::cerr << "Error: Provided file does not meet the size requirement for a ROM-only game.\n";
                error_message = "Provided file does not meet the size requirement for a ROM-only game.";
                return false;
            }
            if (cartridge_ram_size != 0)
            {
                std::cerr << "Error: Provided game ROM contains an invalid RAM size byte for its selected memory bank controller.\n";
                error_message = "Provided game ROM contains an invalid RAM size byte for its selected memory bank controller.";
                return false;
            }
            rom.resize(MemoryBankControllerBase::no_mbc_file_size_bytes, 0);
            ram.resize(0);
            memory_bank_controller = std::make_unique<MemoryBankControllerBase>(rom, ram);
            was_file_load_successful = static_cast<bool>(file.read(reinterpret_cast<char *>(rom.data()), file_length_in_bytes));
            break;
        case MBC1_BYTE:
        case MBC1_WITH_RAM_BYTE:
        case MBC1_WITH_RAM_AND_BATTERY_BYTE:
        {
            if (file_length_in_bytes > MBC1::max_rom_size_bytes)
            {
                std::cerr << "Error: Provided file does not meet the size requirement for an MBC1 game.\n";
                error_message = "Provided file does not meet the size requirement for an MBC1 game.";
                return false;
            }
            if ((cartridge_type == MBC1_BYTE && cartridge_ram_size != 0) ||
                (file_length_in_bytes > MBC1::max_rom_size_in_default_configuration_bytes && cartridge_ram_size > MBC1::max_ram_size_in_large_configuration_bytes))
            {
                std::cerr << "Error: Provided game ROM contains an invalid RAM size byte for its selected memory bank controller.\n";
                error_message = "Provided game ROM contains an invalid RAM size byte for its selected memory bank controller.";
                return false;
            }
            rom.resize(std::bit_ceil(static_cast<uint32_t>(file_length_in_bytes)), 0);
            ram.resize(cartridge_ram_size);
            was_file_load_successful = static_cast<bool>(file.read(reinterpret_cast<char *>(rom.data()), file_length_in_bytes));

            if (was_file_load_successful && rom.size() == MBC1::mbc1m_multi_game_compilation_cart_rom_size_bytes)
            {
                const uint32_t rom_bank_0x10_offset = 0x10 * ROM_BANK_SIZE;
                const bool is_mbc1m_cartridge = std::equal(rom.begin() + rom_bank_0x10_offset + LOGO_START_POSITION, 
                                                           rom.begin() + rom_bank_0x10_offset + LOGO_START_POSITION + LOGO_SIZE,
                                                           std::begin(expected_logo));
                if (is_mbc1m_cartridge)
                {
                    // Convert into standard MBC1 cartridge so normal indexing can be used to read
                    rom.resize(rom.size() * 2);
                    const uint8_t memory_banks_per_sub_rom = 16;

                    for (int sub_rom_number = 3; sub_rom_number >= 0; sub_rom_number--)
                    {
                        for (int memory_bank_number = memory_banks_per_sub_rom - 1; memory_bank_number >= 0; memory_bank_number--)
                        {
                            int previous_global_memory_bank_number = (sub_rom_number * memory_banks_per_sub_rom) + memory_bank_number;
                            uint32_t previous_global_memory_bank_offset = static_cast<uint32_t>(previous_global_memory_bank_number) * ROM_BANK_SIZE;

                            int new_global_memory_bank_number_for_first_copy = ((2 * sub_rom_number) * memory_banks_per_sub_rom) + memory_bank_number;
                            int new_global_memory_bank_number_for_second_copy = ((2 * sub_rom_number + 1) * memory_banks_per_sub_rom) + memory_bank_number;

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
            if (file_length_in_bytes > MBC2::max_rom_size_bytes)
            {
                std::cerr << "Error: Provided file does not meet the size requirement for an MBC2 game.\n";
                error_message = "Provided file does not meet the size requirement for an MBC2 game.";
                return false;
            }
            if (cartridge_ram_size != 0)
            {
                std::cerr << "Error: Provided game ROM contains an invalid RAM size byte for its selected memory bank controller.\n";
                error_message = "Provided game ROM contains an invalid RAM size byte for its selected memory bank controller.";
                return false;
            }
            rom.resize(std::bit_ceil(static_cast<uint32_t>(file_length_in_bytes)), 0);
            ram.resize(MBC2::built_in_ram_size_bytes);
            was_file_load_successful = static_cast<bool>(file.read(reinterpret_cast<char *>(rom.data()), file_length_in_bytes));
            memory_bank_controller = std::make_unique<MBC2>(rom, ram);
            break;
        }
        case MBC5_BYTE:
        case MBC5_WITH_RAM_BYTE:
        case MBC5_WITH_RAM_AND_BATTERY_BYTE:
        case MBC5_WITH_RUMBLE_AND_RAM:
        case MBC5_WITH_RUMBLE_AND_RAM_AND_BATTERY:
        {
            if (file_length_in_bytes > MBC5::max_rom_size_bytes)
            {
                std::cerr << "Error: Provided file does not meet the size requirement for an MBC5 game.\n";
                error_message = "Provided file does not meet the size requirement for an MBC5 game.";
                return false;
            }
            if ((cartridge_type == MBC5_BYTE && cartridge_ram_size != 0) ||
                cartridge_ram_size > MBC5::max_ram_size_bytes)
            {
                std::cerr << "Error: Provided game ROM contains an invalid RAM size byte for its selected memory bank controller.\n";
                error_message = "Provided game ROM contains an invalid RAM size byte for its selected memory bank controller.";
                return false;
            }
            rom.resize(std::bit_ceil(static_cast<uint32_t>(file_length_in_bytes)), 0);
            ram.resize(cartridge_ram_size);
            was_file_load_successful = static_cast<bool>(file.read(reinterpret_cast<char *>(rom.data()), file_length_in_bytes));
            memory_bank_controller = std::make_unique<MBC5>(rom, ram);
            break;
        }
        default:
        {
            std::cerr << std::hex << std::setfill('0')
                      << "Error: Game ROM with cartridge type 0x" << std::setw(2) << static_cast<int>(cartridge_type) << " is not currently supported.\n";

            std::ostringstream output_string_stream;
            output_string_stream << std::hex << std::setfill('0')
                                 << "Game ROM with cartridge type 0x" << std::setw(2) << static_cast<int>(cartridge_type) << " is not currently supported.";
            error_message = output_string_stream.str();
            return false;
        }
    }

    if (!was_file_load_successful)
    {
        std::cerr << "Error: Could not read game rom file " << file_path << ".\n";
        error_message = "Could not read game rom file " + file_path.string();
    }
    return was_file_load_successful;
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
