#include <algorithm>
#include <array>
#include <bit>
#include <cmath>
#include <iomanip>
#include <iostream>

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
    if (address >= MemoryBankControllerBase::ROM_ONLY_WITH_NO_MBC_FILE_SIZE)
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
    number_of_rom_banks = rom.size() >> ROM_BANK_SIZE_POWER_OF_TWO;
    number_of_ram_banks = ram.size() >> RAM_BANK_SIZE_POWER_OF_TWO;
}

uint8_t MBC1::read_byte(uint16_t address)
{
    if (address < 0x4000)
    {
        const uint8_t selected_rom_bank_number = (banking_mode == 1)
            ? (ram_bank_number_or_upper_two_bits_of_rom_bank_number << 5) & (number_of_rom_banks - 1)
            : 0;
        const uint32_t selected_rom_bank_starting_address = selected_rom_bank_number << std::countr_zero(ROM_BANK_SIZE);
        const uint32_t address_to_read = selected_rom_bank_starting_address | address;
        return cartridge_rom[address_to_read];
    }
    else if (address < 0x8000)
    {
        const uint8_t selected_rom_bank_number = (ram_bank_number_or_upper_two_bits_of_rom_bank_number << 5 | lower_five_bits_of_rom_bank_number) &
                                                 (number_of_rom_banks - 1);
        const uint32_t selected_rom_bank_starting_address = selected_rom_bank_number << std::countr_zero(ROM_BANK_SIZE);
        const uint16_t selected_address_within_rom_bank = address & (ROM_BANK_SIZE - 1);
        const uint32_t address_to_read = selected_rom_bank_starting_address | selected_address_within_rom_bank;
        return cartridge_rom[address_to_read];
    }
    else if (address >= 0xa000 && address < 0xc000)
    {
        if (!is_ram_enabled)
        {
            return 0xff;
        }
        const uint8_t selected_ram_bank_number = (banking_mode == 1)
            ? ram_bank_number_or_upper_two_bits_of_rom_bank_number & (number_of_ram_banks - 1)
            : 0;
        const uint32_t selected_rom_bank_starting_address = selected_ram_bank_number << std::countr_zero(RAM_BANK_SIZE);
        const uint16_t selected_address_within_rom_bank = address & (RAM_BANK_SIZE - 1);
        const uint32_t address_to_read = selected_rom_bank_starting_address | selected_address_within_rom_bank;
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
        lower_five_bits_of_rom_bank_number = std::max(value & 0b11111, MINIMUM_ALLOWABLE_ROM_BANK_NUMBER);
    }
    else if (address < 0x6000)
    {
        ram_bank_number_or_upper_two_bits_of_rom_bank_number = (value & 0b11);
    }
    else if (address < 0x8000)
    {
        banking_mode = (value & 1);
    }
    else if (address >= 0xa000 && address < 0xc000)
    {
        if (!is_ram_enabled)
        {
            return;
        }
        const uint8_t selected_ram_bank_number = (banking_mode == 1)
            ? ram_bank_number_or_upper_two_bits_of_rom_bank_number & (number_of_ram_banks - 1)
            : 0;
        const uint32_t address_to_write = (address & 0x1fff) | (selected_ram_bank_number << 13);
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
        const uint32_t selected_rom_bank_starting_address = selected_rom_bank_number << std::countr_zero(ROM_BANK_SIZE);
        const uint16_t selected_address_within_rom_bank = address & (ROM_BANK_SIZE - 1);
        const uint32_t address_to_read = (selected_rom_bank_starting_address | selected_address_within_rom_bank) & (cartridge_rom.size() - 1);
        return cartridge_rom[address_to_read];
    }
    else if (address >= 0xa000 && address < 0xc000)
    {
        if (!is_ram_enabled)
        {
            return 0xff;
        }
        const uint16_t address_to_read = address & (MBC2::BUILT_IN_RAM_SIZE - 1);
        return cartridge_ram[address_to_read];
    }
    throw std::runtime_error("Attemped to read from out of bounds address " + std::to_string(address) + " in the cartridge's ROM or RAM. Exiting.");
}

void MBC2::write_byte(uint16_t address, uint8_t value)
{
    if (address < 0x4000)
    {
        const bool does_write_control_rom = is_bit_set(address, static_cast<uint16_t>(8));
        if (does_write_control_rom)
        {
            const int requested_rom_bank_number_with_wrapping = value & (MAX_NUMBER_OF_ROM_BANKS - 1);
            selected_rom_bank_number = std::max(requested_rom_bank_number_with_wrapping, MINIMUM_ALLOWABLE_ROM_BANK_NUMBER);
            is_ram_enabled = false;
        }
        else
        {
            selected_rom_bank_number = MBC2::MINIMUM_ALLOWABLE_ROM_BANK_NUMBER;
            is_ram_enabled = ((value & 0x0f) == 0x0a);
        }
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
        const uint16_t address_to_write = address & (MBC2::BUILT_IN_RAM_SIZE - 1);
        const uint8_t value_with_undefined_upper_nibble = value | 0xf0;
        cartridge_ram[address_to_write] = value_with_undefined_upper_nibble;
    }
    else
        throw std::runtime_error("Attemped to write to out of bounds address " + std::to_string(address) + " in the cartridge's ROM or RAM. Exiting.");
}

MBC3::MBC3(std::vector<uint8_t> &rom, std::vector<uint8_t> &ram)
    : MemoryBankControllerBase{rom, ram}
{
    number_of_rom_banks = rom.size() >> ROM_BANK_SIZE_POWER_OF_TWO;
    number_of_ram_banks = ram.size() >> RAM_BANK_SIZE_POWER_OF_TWO;
}

uint8_t MBC3::read_byte(uint16_t address)
{
    if (address < 0x4000)
    {
        return cartridge_rom[address];
    }
    else if (address < 0x8000)
    {
        const uint32_t selected_rom_bank_starting_address = selected_rom_bank_number << std::countr_zero(ROM_BANK_SIZE);
        const uint16_t address_within_rom_bank = address & (ROM_BANK_SIZE - 1);
        const uint32_t address_to_read = selected_rom_bank_starting_address | address_within_rom_bank;
        return cartridge_rom[address_to_read];
    }
    else if (address >= 0xa000 && address < 0xc000)
    {
        if (selected_ram_bank_number_or_real_time_clock_register_select < 0x08)
        {
            const uint32_t selected_ram_bank_starting_address = selected_ram_bank_number_or_real_time_clock_register_select << std::countr_zero(RAM_BANK_SIZE);
            const uint16_t selected_address_within_ram_bank = address & (RAM_BANK_SIZE - 1);
            const uint32_t address_to_read = (selected_ram_bank_starting_address | selected_address_within_ram_bank) & static_cast<uint32_t>(cartridge_ram.size() - 1);
            return cartridge_ram[address_to_read];
        }
        else
        {
            switch (selected_ram_bank_number_or_real_time_clock_register_select)
            {
                case 0x08:
                    return real_time_clock.seconds_counter;
                case 0x09:
                    return real_time_clock.minutes_counter;
                case 0x0a:
                    return real_time_clock.hours_counter;
                case 0x0b:
                    return real_time_clock.days_counter = real_time_clock.days_counter & 0x00ff;
                case 0x0c:
                {
                    uint8_t result = 0;

                    if (is_bit_set(real_time_clock.days_counter, 8))
                    {
                        result |= 1;
                    }
                    if (real_time_clock.is_halted)
                    {
                        result |= (1 << 6);
                    }
                    if (real_time_clock.is_day_counter_carry_set)
                    {
                        result |= (1 << 7);
                    }
                    return result;
                }
                default:
                    return 0xff;
            }
        }
    }
    throw std::runtime_error("Attemped to read from out of bounds address " + std::to_string(address) + " in the cartridge's ROM or RAM. Exiting.");
}

void MBC3::write_byte(uint16_t address, uint8_t value)
{
    if (address < 0x2000)
    {
        are_ram_and_real_time_clock_enabled = ((value & 0x0f) == 0x0a);
    }
    else if (address < 0x4000)
    {
        selected_rom_bank_number = std::max((value & 0b01111111) & (number_of_rom_banks - 1), MINIMUM_ALLOWABLE_ROM_BANK_NUMBER);
    }
    else if (address < 0x6000)
    {
        selected_ram_bank_number_or_real_time_clock_register_select = value;
    }
    else if (address < 0x8000)
    {
        if (value == 0x01 && real_time_clock.latch_clock_data == 0x00)
        {
            real_time_clock.are_time_counters_latched = !real_time_clock.are_time_counters_latched;
        }
        real_time_clock.latch_clock_data = value;
    }
    else if (address >= 0xa000 && address < 0xc000)
    {
        if (!are_ram_and_real_time_clock_enabled)
        {
            return;
        }

        if (selected_ram_bank_number_or_real_time_clock_register_select < 0x08)
        {
            const uint32_t selected_ram_bank_starting_address = (selected_ram_bank_number_or_real_time_clock_register_select & (number_of_ram_banks - 1)) << std::countr_zero(RAM_BANK_SIZE);
            const uint16_t selected_address_within_ram_bank = address & (RAM_BANK_SIZE - 1);
            const uint32_t address_to_read = (selected_ram_bank_starting_address | selected_address_within_ram_bank) & static_cast<uint32_t>(cartridge_ram.size() - 1);
            cartridge_ram[address_to_read] = value;
        }
        else
        {
            switch (selected_ram_bank_number_or_real_time_clock_register_select)
            {
                case 0x08:
                    real_time_clock.seconds_counter = value % 60;
                    break;
                case 0x09:
                    real_time_clock.minutes_counter = value % 60;
                    break;
                case 0x0a:
                    real_time_clock.hours_counter = value % 24;
                    break;
                case 0x0b:
                    real_time_clock.days_counter = (real_time_clock.days_counter & 0xff00) | value;
                    break;
                case 0x0c:
                    set_bit(real_time_clock.days_counter, 8, is_bit_set(value, 0));
                    real_time_clock.is_halted = is_bit_set(6, value);
                    real_time_clock.is_day_counter_carry_set = is_bit_set(7, value);
                    break;
            }
        }
    }
    else
        throw std::runtime_error("Attemped to write to an out of bounds address in the cartridge's ROM or RAM. Exiting.");
}

MBC5::MBC5(std::vector<uint8_t> &rom, std::vector<uint8_t> &ram)
    : MemoryBankControllerBase{rom, ram}
{
    number_of_rom_banks = rom.size() >> ROM_BANK_SIZE_POWER_OF_TWO;
    number_of_ram_banks = ram.size() >> RAM_BANK_SIZE_POWER_OF_TWO;
}

uint8_t MBC5::read_byte(uint16_t address)
{
    if (address < 0x4000)
    {
        return cartridge_rom[address];
    }
    else if (address < 0x8000)
    {
        const uint32_t selected_rom_bank_starting_address = selected_rom_bank_number << std::countr_zero(ROM_BANK_SIZE);
        const uint16_t address_within_rom_bank = address & (ROM_BANK_SIZE - 1);
        const uint32_t address_to_read = selected_rom_bank_starting_address | address_within_rom_bank;
        return cartridge_rom[address_to_read];
    }
    else if (address >= 0xa000 && address < 0xc000)
    {
        if (!is_ram_enabled)
        {
            return 0xff;
        }
        const uint32_t selected_ram_bank_starting_address = selected_ram_bank_number << std::countr_zero(RAM_BANK_SIZE);
        const uint16_t selected_address_within_ram_bank = address & (RAM_BANK_SIZE - 1);
        const uint32_t address_to_read = (selected_ram_bank_starting_address | selected_address_within_ram_bank) & static_cast<uint32_t>(cartridge_ram.size() - 1);
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
        selected_rom_bank_number = ((selected_rom_bank_number & 0xff00) | bits_0_to_7_of_rom_bank_number) & (number_of_rom_banks - 1);
    }
    else if (address < 0x4000)
    {
        const uint16_t bit_8_of_rom_bank_number = (value & 1) << 8;
        selected_rom_bank_number = ((selected_rom_bank_number & 0x00ff) | bit_8_of_rom_bank_number) & (number_of_rom_banks - 1);
    }
    else if (address < 0x6000)
    {
        selected_ram_bank_number = value & (number_of_ram_banks - 1);
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
        const uint32_t selected_ram_bank_starting_address = selected_ram_bank_number << std::countr_zero(RAM_BANK_SIZE);
        const uint16_t selected_address_within_ram_bank = address & (RAM_BANK_SIZE - 1);
        const uint32_t address_to_write = selected_ram_bank_starting_address | selected_address_within_ram_bank;
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
    rom.resize(MemoryBankControllerBase::ROM_ONLY_WITH_NO_MBC_FILE_SIZE, 0);
    ram.resize(0);
    memory_bank_controller = std::make_unique<MemoryBankControllerBase>(rom, ram);
}

bool GameCartridgeSlot::try_load_file(const std::filesystem::path &file_path, std::ifstream &file, std::streamsize file_length_in_bytes, std::string &error_message)
{
    if (file_length_in_bytes < MemoryBankControllerBase::ROM_ONLY_WITH_NO_MBC_FILE_SIZE)
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
        {
            if (file_length_in_bytes > MemoryBankControllerBase::ROM_ONLY_WITH_NO_MBC_FILE_SIZE)
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
                std::cerr << "Error: Provided file does not meet the size requirement for an MBC1 game.\n";
                error_message = "Provided file does not meet the size requirement for an MBC1 game.";
                return false;
            }
            if ((cartridge_type == MBC1_BYTE && cartridge_ram_size != 0) ||
                (file_length_in_bytes > MBC1::MAX_ROM_SIZE_IN_DEFAULT_CONFIGURATION && cartridge_ram_size > MBC1::MAX_RAM_SIZE_IN_LARGE_CONFIGURATION))
            {
                std::cerr << "Error: Provided game ROM contains an invalid RAM size byte for its selected memory bank controller.\n";
                error_message = "Provided game ROM contains an invalid RAM size byte for its selected memory bank controller.";
                return false;
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
            ram.resize(MBC2::BUILT_IN_RAM_SIZE, 0xf0);
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
                std::cerr << "Error: Provided file does not meet the size requirement for an MBC2 game.\n";
                error_message = "Provided file does not meet the size requirement for an MBC2 game.";
                return false;
            }
            if (((cartridge_type == MBC3_BYTE || cartridge_ram_size == MBC3_WITH_TIMER_AND_BATTERY_BYTE) && cartridge_ram_size != 0) ||
                cartridge_ram_size > MBC3::MAX_RAM_SIZE)
            {
                std::cerr << "Error: Provided game ROM contains an invalid RAM size byte for its selected memory bank controller.\n";
                error_message = "Provided game ROM contains an invalid RAM size byte for its selected memory bank controller.";
                return false;
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
                std::cerr << "Error: Provided file does not meet the size requirement for an MBC5 game.\n";
                error_message = "Provided file does not meet the size requirement for an MBC5 game.";
                return false;
            }
            if ((cartridge_type == MBC5_BYTE && cartridge_ram_size != 0) ||
                cartridge_ram_size > MBC5::MAX_RAM_SIZE)
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
