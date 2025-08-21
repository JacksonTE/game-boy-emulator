#include <bit>
#include <cmath>
#include <iomanip>
#include <iostream>
#include <string>

#include "bitwise_utilities.h"
#include "memory_bank_controllers.h"

namespace GameBoyCore
{

MemoryBankControllerBase::MemoryBankControllerBase(std::vector<uint8_t>& rom, std::vector<uint8_t>& ram)
    : cartridge_rom{rom},
      cartridge_ram{ram}
{
}

uint8_t	MemoryBankControllerBase::read_byte(uint16_t address)
{
    if (address >= MemoryBankControllerBase::ROM_ONLY_WITH_NO_MBC_FILE_SIZE)
    {
        std::cout << std::hex << std::setfill('0') 
                  << "Attemped to read from non-existent cartridge RAM at address " << std::setw(4) << address << "Returning 0xFF as a fallback.";
        return 0xFF;
    }
    return cartridge_rom[address];
}

void MemoryBankControllerBase::write_byte(uint16_t address, uint8_t value)
{
    std::cout << std::hex << std::setfill('0')
              << "Attempted to write to read only address 0x" << std::setw(4) << address << " in a ROM-only cartridge. No operation will occur.\n";
}

MBC1::MBC1(std::vector<uint8_t>& rom, std::vector<uint8_t>& ram)
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
    else if (address >= 0xA000 && address < 0xC000)
    {
        if (!is_ram_enabled)
        {
            return 0xFF;
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
        is_ram_enabled = ((value & 0x0F) == 0x0A);
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
    else if (address >= 0xA000 && address < 0xC000)
    {
        if (!is_ram_enabled)
        {
            return;
        }
        const uint8_t selected_ram_bank_number = (banking_mode == 1)
            ? ram_bank_number_or_upper_two_bits_of_rom_bank_number & (number_of_ram_banks - 1)
            : 0;
        const uint32_t address_to_write = (address & 0x1FFF) | (selected_ram_bank_number << 13);
        cartridge_ram[address_to_write] = value;
    }
    else
        throw std::runtime_error("Attemped to write to an out of bounds address in the cartridge's ROM or RAM. Exiting.");
}

MBC2::MBC2(std::vector<uint8_t>& rom, std::vector<uint8_t>& ram)
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
    else if (address >= 0xA000 && address < 0xC000)
    {
        if (!is_ram_enabled)
        {
            return 0xFF;
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
            is_ram_enabled = ((value & 0x0F) == 0x0A);
        }
    }
    else if (address < 0x8000)
    {
        std::cout << "Attemped to write to out of bounds address " + std::to_string(address) + " in the cartridge's ROM. No operation will occur.\n";
    }
    else if (address >= 0xA000 && address < 0xC000)
    {
        if (!is_ram_enabled)
        {
            return;
        }
        const uint16_t address_to_write = address & (MBC2::BUILT_IN_RAM_SIZE - 1);
        const uint8_t value_with_undefined_upper_nibble = value | 0xF0;
        cartridge_ram[address_to_write] = value_with_undefined_upper_nibble;
    }
    else
        throw std::runtime_error("Attemped to write to out of bounds address " + std::to_string(address) + " in the cartridge's ROM or RAM. Exiting.");
}

MBC3::MBC3(std::vector<uint8_t>& rom, std::vector<uint8_t>& ram)
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
    else if (address >= 0xA000 && address < 0xC000)
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
                case 0x0A:
                    return real_time_clock.hours_counter;
                case 0x0B:
                    return real_time_clock.days_counter = real_time_clock.days_counter & 0x00FF;
                case 0x0C:
                {
                    uint8_t result = 0;
                    set_bit(result, 0, is_bit_set(real_time_clock.days_counter, 8));
                    set_bit(result, 6, real_time_clock.is_halted);
                    set_bit(result, 7, real_time_clock.is_day_counter_carry_set);
                    return result;
                }
                default:
                    return 0xFF;
            }
        }
    }
    throw std::runtime_error("Attemped to read from out of bounds address " + std::to_string(address) + " in the cartridge's ROM or RAM. Exiting.");
}

void MBC3::write_byte(uint16_t address, uint8_t value)
{
    if (address < 0x2000)
    {
        are_ram_and_real_time_clock_enabled = ((value & 0x0F) == 0x0A);
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
    else if (address >= 0xA000 && address < 0xC000)
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
                case 0x0A:
                    real_time_clock.hours_counter = value % 24;
                    break;
                case 0x0B:
                    real_time_clock.days_counter = (real_time_clock.days_counter & 0xFF00) | value;
                    break;
                case 0x0C:
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

MBC5::MBC5(std::vector<uint8_t>& rom, std::vector<uint8_t>& ram)
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
    else if (address >= 0xA000 && address < 0xC000)
    {
        if (!is_ram_enabled)
        {
            return 0xFF;
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
        is_ram_enabled = ((value & 0x0F) == 0x0A);
    }
    else if (address < 0x3000)
    {
        const uint8_t bits_0_to_7_of_rom_bank_number = value;
        selected_rom_bank_number = ((selected_rom_bank_number & 0xFF00) | bits_0_to_7_of_rom_bank_number) & (number_of_rom_banks - 1);
    }
    else if (address < 0x4000)
    {
        const uint16_t bit_8_of_rom_bank_number = (value & 1) << 8;
        selected_rom_bank_number = ((selected_rom_bank_number & 0x00FF) | bit_8_of_rom_bank_number) & (number_of_rom_banks - 1);
    }
    else if (address < 0x6000)
    {
        selected_ram_bank_number = value & (number_of_ram_banks - 1);
    }
    else if (address < 0x8000)
    {
        std::cout << "Attemped to write to out of bounds address " + std::to_string(address) + " in the cartridge's ROM. No operation will occur.\n";
    }
    else if (address >= 0xA000 && address < 0xC000)
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

} // namespace GameBoyCore
