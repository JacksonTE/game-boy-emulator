#include <algorithm>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>

#include "bitwise_utilities.h"
#include "memory_management_unit.h"

namespace GameBoy
{

MemoryManagementUnit::MemoryManagementUnit(Timer &timer_reference, PixelProcessingUnit &pixel_processing_unit_reference)
    : timer{timer_reference},
      pixel_processing_unit{pixel_processing_unit_reference}
{
    bootrom = std::make_unique<uint8_t[]>(BOOTROM_SIZE);
    rom_bank_00 = std::make_unique<uint8_t[]>(ROM_BANK_SIZE);
    rom_bank_01 = std::make_unique<uint8_t[]>(ROM_BANK_SIZE);
    external_ram = std::make_unique<uint8_t[]>(EXTERNAL_RAM_SIZE);
    work_ram = std::make_unique<uint8_t[]>(WORK_RAM_SIZE);
    unmapped_input_output_registers = std::make_unique<uint8_t[]>(INPUT_OUTPUT_REGISTERS_SIZE);
    high_ram = std::make_unique<uint8_t[]>(HIGH_RAM_SIZE);

    std::fill_n(bootrom.get(), BOOTROM_SIZE, 0);
    std::fill_n(rom_bank_00.get(), ROM_BANK_SIZE, 0);
    std::fill_n(rom_bank_01.get(), ROM_BANK_SIZE, 0);
    std::fill_n(external_ram.get(), EXTERNAL_RAM_SIZE, 0);
    std::fill_n(work_ram.get(), WORK_RAM_SIZE, 0);
    std::fill_n(unmapped_input_output_registers.get(), INPUT_OUTPUT_REGISTERS_SIZE, 0);
    std::fill_n(high_ram.get(), HIGH_RAM_SIZE, 0);
}

void MemoryManagementUnit::reset_state()
{
    std::fill_n(rom_bank_00.get(), ROM_BANK_SIZE, 0);
    std::fill_n(rom_bank_01.get(), ROM_BANK_SIZE, 0);
    std::fill_n(external_ram.get(), EXTERNAL_RAM_SIZE, 0);
    std::fill_n(work_ram.get(), WORK_RAM_SIZE, 0);
    std::fill_n(unmapped_input_output_registers.get(), INPUT_OUTPUT_REGISTERS_SIZE, 0);
    std::fill_n(high_ram.get(), HIGH_RAM_SIZE, 0);
}

void MemoryManagementUnit::set_post_boot_state()
{
    bootrom_status = 1;
    write_byte(0xff00, 0xcf);
    write_byte(0xff01, 0x00);
    write_byte(0xff02, 0x7e);
    interrupt_flag_if = 0xe1;
    write_byte(0xff10, 0x80);
    write_byte(0xff11, 0xbf);
    write_byte(0xff12, 0xf3);
    write_byte(0xff13, 0xff);
    write_byte(0xff14, 0xbf);
    write_byte(0xff16, 0x3f);
    write_byte(0xff17, 0x00);
    write_byte(0xff18, 0xff);
    write_byte(0xff19, 0xbf);
    write_byte(0xff1a, 0x7f);
    write_byte(0xff1b, 0xff);
    write_byte(0xff1c, 0x9f);
    write_byte(0xff1d, 0xff);
    write_byte(0xff1e, 0xbf);
    write_byte(0xff20, 0xff);
    write_byte(0xff21, 0x00);
    write_byte(0xff22, 0x00);
    write_byte(0xff23, 0xbf);
    write_byte(0xff24, 0x77);
    write_byte(0xff25, 0xf3);
    write_byte(0xff26, 0xf1);
    interrupt_enable_ie = 0x00;
}

bool MemoryManagementUnit::try_load_file(uint16_t number_of_bytes_to_load, const std::filesystem::path &file_path, bool is_bootrom_file)
{
    std::ifstream file(file_path, std::ios::binary | std::ios::ate);
    if (!file)
    {
        std::cerr << "Error: file not found at " << file_path << ".\n";
        return false;
    }

    std::streamsize file_length_in_bytes = file.tellg();
    if (file_length_in_bytes < static_cast<std::streamsize>(number_of_bytes_to_load))
    {
        std::cerr << std::hex;
        std::cerr << "Error: file size (" << file_length_in_bytes << ") is less than requested number of bytes to load (" << number_of_bytes_to_load << ").\n";
        return false;
    }

    if (number_of_bytes_to_load > (is_bootrom_file ? BOOTROM_SIZE : 2 * ROM_BANK_SIZE))
    {
        std::cerr << std::hex << std::setfill('0');
        std::cerr << "Error: insufficient space from starting address (" << std::setw(4) << 0x0000
                  << ") to load requested number of bytes (" << number_of_bytes_to_load << ").\n";
        return false;
    }

    file.seekg(0, std::ios::beg);
    bool was_file_load_successful = true;

    if (is_bootrom_file)
    {
        was_file_load_successful &= static_cast<bool>(file.read(reinterpret_cast<char *>(bootrom.get()), number_of_bytes_to_load));
    }
    else
    {
        uint16_t first_rom_bank_bytes_count = std::min(number_of_bytes_to_load, ROM_BANK_SIZE);
        uint16_t second_rom_bank_bytes_count = number_of_bytes_to_load - first_rom_bank_bytes_count;
        
        if (first_rom_bank_bytes_count > 0)
        {
            was_file_load_successful &= static_cast<bool>(file.read(reinterpret_cast<char *>(rom_bank_00.get()), first_rom_bank_bytes_count));
        }
        if (second_rom_bank_bytes_count > 0)
        {
            was_file_load_successful &= static_cast<bool>(file.read(reinterpret_cast<char *>(rom_bank_01.get()), second_rom_bank_bytes_count));
        }
    }

    if (!was_file_load_successful)
    {
        std::cerr << "Error: could not read file " << file_path << ".\n";
    }
    return was_file_load_successful;
}

uint8_t MemoryManagementUnit::read_byte(uint16_t address) const
{
    if (bootrom_status == 0 && address < BOOTROM_SIZE)
    {
        if (bootrom == nullptr)
        {
            std::cerr << std::hex << std::setfill('0');
            std::cerr << "Warning: attempted read from bootrom address (" << std::setw(4) << address << ") pointing to an unallocated bootrom. "
                      << "Returning 0xff as a fallback.\n";
            return 0xff;
        }
        return bootrom[address];
    }
    else if (address >= ROM_BANK_00_START && address < ROM_BANK_00_START + ROM_BANK_SIZE)
    {
        const uint16_t local_address = address - ROM_BANK_00_START;
        return rom_bank_00[local_address];
    }
    else if (address >= ROM_BANK_01_START && address < ROM_BANK_01_START + ROM_BANK_SIZE)
    {
        const uint16_t local_address = address - ROM_BANK_01_START;
        return rom_bank_01[local_address];
    }
    else if (address >= VIDEO_RAM_START && address < VIDEO_RAM_START + VIDEO_RAM_SIZE)
    {
        return pixel_processing_unit.read_byte_video_ram(address);
    }
    else if (address >= EXTERNAL_RAM_START && address < EXTERNAL_RAM_START + EXTERNAL_RAM_SIZE)
    {
        const uint16_t local_address = address - EXTERNAL_RAM_START;
        return external_ram[local_address];
    }
    else if (address >= WORK_RAM_START && address < WORK_RAM_START + WORK_RAM_SIZE)
    {
        const uint16_t local_address = address - WORK_RAM_START;
        return work_ram[local_address];
    }
    else if (address >= ECHO_RAM_START && address < ECHO_RAM_START + ECHO_RAM_SIZE)
    {
        const uint16_t local_address = address - ECHO_RAM_START;
        return work_ram[local_address];
    }
    else if (address >= OBJECT_ATTRIBUTE_MEMORY_START && address < OBJECT_ATTRIBUTE_MEMORY_START + OBJECT_ATTRIBUTE_MEMORY_SIZE)
    {
        return pixel_processing_unit.read_byte_object_attribute_memory(address);
    }
    else if (address >= UNUSABLE_MEMORY_START && address < UNUSABLE_MEMORY_START + UNUSABLE_MEMORY_SIZE)
    {
        // TODO eventually add OAM corruption check
        return 0x00;
    }
    else if (address >= INPUT_OUTPUT_REGISTERS_START && address < INPUT_OUTPUT_REGISTERS_START + INPUT_OUTPUT_REGISTERS_SIZE)
    {
        switch (address)
        {
            case 0xff04:
                return timer.read_div();
            case 0xff05:
                return timer.read_tima();
            case 0xff06:
                return timer.read_tma();
            case 0xff07:
                return timer.read_tac();
            case 0xff0f:
                return interrupt_flag_if | 0b11100000;
            case 0xff40:
                return pixel_processing_unit.read_lcd_control_lcdc();
            case 0xff41:
                return pixel_processing_unit.read_lcd_status_stat();
            case 0xff42:
                return pixel_processing_unit.viewport_y_position_scy;
            case 0xff43:
                return pixel_processing_unit.viewport_x_position_scx;
            case 0xff44:
                return pixel_processing_unit.read_lcd_y_coordinate_ly();
            case 0xff45:
                return pixel_processing_unit.lcd_y_coordinate_compare_lyc;
            case 0xff46:
                return pixel_processing_unit.object_attribute_memory_direct_memory_access_dma;
            case 0xff47:
                return pixel_processing_unit.background_palette_bgp;
            case 0xff48:
                return pixel_processing_unit.object_palette_0_obp0;
            case 0xff49:
                return pixel_processing_unit.object_palette_1_obp1;
            case 0xff4a:
                return pixel_processing_unit.window_y_position_wy;
            case 0xff4b:
                return pixel_processing_unit.window_x_position_plus_7_wx;
            case 0xff50:
                return bootrom_status;
            default:
                const uint16_t local_address = address - INPUT_OUTPUT_REGISTERS_START;
                return unmapped_input_output_registers[local_address];
        }
    }
    else if (address >= HIGH_RAM_START && address < HIGH_RAM_START + HIGH_RAM_SIZE)
    {
        const uint16_t local_address = address - HIGH_RAM_START;
        return high_ram[local_address];
    }
    else
        return interrupt_enable_ie;
}

void MemoryManagementUnit::write_byte(uint16_t address, uint8_t value)
{
    if (address >= ROM_BANK_00_START && address < ROM_BANK_00_START + ROM_BANK_SIZE)
    {
        wrote_to_read_only_address(address);
    }
    else if (address >= ROM_BANK_01_START && address < ROM_BANK_01_START + ROM_BANK_SIZE)
    {
        wrote_to_read_only_address(address);
    }
    else if (address >= VIDEO_RAM_START && address < VIDEO_RAM_START + VIDEO_RAM_SIZE)
    {
        pixel_processing_unit.write_byte_video_ram(address, value);
    }
    else if (address >= EXTERNAL_RAM_START && address < EXTERNAL_RAM_START + EXTERNAL_RAM_SIZE)
    {
        const uint16_t local_address = address - EXTERNAL_RAM_START;
        external_ram[local_address] = value;
    }
    else if (address >= WORK_RAM_START && address < WORK_RAM_START + WORK_RAM_SIZE)
    {
        const uint16_t local_address = address - WORK_RAM_START;
        work_ram[local_address] = value;
    }
    else if (address >= ECHO_RAM_START && address < ECHO_RAM_START + ECHO_RAM_SIZE)
    {
        const uint16_t local_address = address - ECHO_RAM_START;
        work_ram[local_address] = value;
    }
    else if (address >= OBJECT_ATTRIBUTE_MEMORY_START && address < OBJECT_ATTRIBUTE_MEMORY_START + OBJECT_ATTRIBUTE_MEMORY_SIZE)
    {
        pixel_processing_unit.write_byte_object_attribute_memory(address, value);
    }
    else if (address >= UNUSABLE_MEMORY_START && address < UNUSABLE_MEMORY_START + UNUSABLE_MEMORY_SIZE)
    {
        // TODO check if OAM corruption is relevant here
        std::cout << "Attempted to write to unusable address " << address << ". No write will occur.\n";
    }
    else if (address >= INPUT_OUTPUT_REGISTERS_START && address < INPUT_OUTPUT_REGISTERS_START + INPUT_OUTPUT_REGISTERS_SIZE)
    {
        switch (address)
        {
            case 0xff04:
                timer.write_div(value);
                return;
            case 0xff05:
                timer.write_tima(value);
                return;
            case 0xff06:
                timer.write_tma(value);
                return;
            case 0xff07:
                timer.write_tac(value);
                return;
            case 0xff0f:
                interrupt_flag_if = value | 0b11100000;
                return;
            case 0xff40:
                pixel_processing_unit.write_lcd_control_lcdc(value);
                return;
            case 0xff41:
                pixel_processing_unit.write_lcd_status_stat(value);
                return;
            case 0xff42:
                pixel_processing_unit.viewport_y_position_scy = value;
                return;
            case 0xff43:
                pixel_processing_unit.viewport_x_position_scx = value;
                return;
            case 0xff44:
                wrote_to_read_only_address(0xff44);
                return;
            case 0xff45:
                pixel_processing_unit.lcd_y_coordinate_compare_lyc = value;
                return;
            case 0xff46:
                pixel_processing_unit.object_attribute_memory_direct_memory_access_dma = value;
                return;
            case 0xff47:
                pixel_processing_unit.background_palette_bgp = value;
                return;
            case 0xff48:
                pixel_processing_unit.object_palette_0_obp0 = value;
                return;
            case 0xff49:
                pixel_processing_unit.object_palette_1_obp1 = value;
                return;
            case 0xff4a:
                pixel_processing_unit.window_y_position_wy = value;
                return;
            case 0xff4b:
                pixel_processing_unit.window_x_position_plus_7_wx = value;
                return;
            case 0xff50:
                bootrom_status = value;
                return;
            default:
                const uint16_t local_address = address - INPUT_OUTPUT_REGISTERS_START;
                unmapped_input_output_registers[local_address] = value;
                return;
        }
    }
    else if (address >= HIGH_RAM_START && address < HIGH_RAM_START + HIGH_RAM_SIZE)
    {
        const uint16_t local_address = address - HIGH_RAM_START;
        high_ram[local_address] = value;
    }
    else
        interrupt_enable_ie = value;
}

void MemoryManagementUnit::request_interrupt(uint8_t interrupt_flag_mask)
{
    update_flag(interrupt_flag_if, interrupt_flag_mask, true);
}

void MemoryManagementUnit::clear_interrupt_flag_bit(uint8_t interrupt_flag_mask)
{
    update_flag(interrupt_flag_if, interrupt_flag_mask, false);
}

uint8_t MemoryManagementUnit::get_pending_interrupt_mask() const
{
    for (uint8_t i = 0; i < NUMBER_OF_INTERRUPT_TYPES; i++)
    {
        uint8_t interrupt_flag_mask = 1 << i;
        const bool is_interrupt_type_requested = is_flag_set(interrupt_flag_if, interrupt_flag_mask);
        const bool is_interrupt_type_enabled = is_flag_set(interrupt_enable_ie, interrupt_flag_mask);

        if (is_interrupt_type_requested && is_interrupt_type_enabled)
            return interrupt_flag_mask;
    }
    return 0x00;
}

void MemoryManagementUnit::wrote_to_read_only_address(uint16_t address) const
{
    std::cout << "Attempted to write to read only address " << address << ". No write will occur.\n";
}

} //namespace GameBoy
