#include <filesystem>
#include <fstream>
#include <functional>
#include <iomanip>
#include <iostream>
#include "memory_management_unit.h"

namespace GameBoy
{

MemoryManagementUnit::MemoryManagementUnit(PixelProcessingUnit &pixel_processing_unit_reference)
    : timer{[this](uint8_t interrupt_flag_mask) { this->request_interrupt(interrupt_flag_mask); }},
	  pixel_processing_unit{pixel_processing_unit_reference}
{
    placeholder_memory = std::make_unique<uint8_t[]>(MEMORY_SIZE);
    std::fill_n(placeholder_memory.get(), MEMORY_SIZE, 0);
}

void MemoryManagementUnit::reset_state()
{
    std::fill_n(placeholder_memory.get(), MEMORY_SIZE, 0);
    bootrom.reset();
}

void MemoryManagementUnit::set_post_boot_state()
{
    bootrom_status = 1;
    write_byte(0xff00, 0xcf);
    write_byte(0xff01, 0x00);
    write_byte(0xff02, 0x7e);
    write_byte(0xff04, 0xab);
    write_byte(0xff05, 0x00);
    write_byte(0xff06, 0x00);
    write_byte(0xff07, 0xf8);
    write_byte(0xff0f, 0xe1);
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
    write_byte(0xff40, 0x90);
    write_byte(0xff41, 0x85);
    write_byte(0xff42, 0x00);
    write_byte(0xff43, 0x00);
    write_byte(0xff44, 0x00);
    write_byte(0xff45, 0x00);
    write_byte(0xff46, 0xff);
    write_byte(0xff47, 0xfc);
    write_byte(0xff48, 0x00);
    write_byte(0xff49, 0x00);
    write_byte(0xff4a, 0x00);
    write_byte(0xff4b, 0x00);
    write_byte(0xffff, 0x00);
}

bool MemoryManagementUnit::try_load_file(uint16_t address, uint32_t number_of_bytes_to_load, std::filesystem::path file_path, bool is_bootrom_file)
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

    if (address + number_of_bytes_to_load > (is_bootrom_file ? BOOTROM_SIZE : COLLECTIVE_ROM_BANK_SIZE))
	{
        std::cerr << std::hex << std::setfill('0');
        std::cerr << "Error: insufficient space from starting address (" << std::setw(4) << address
		          << ") to load requested number of bytes (" << number_of_bytes_to_load << ").\n";
        return false;
    }

    file.seekg(0, std::ios::beg);

    if (is_bootrom_file)
	{
        if (bootrom == nullptr)
            bootrom = std::make_unique<uint8_t[]>(BOOTROM_SIZE);

        std::fill_n(bootrom.get(), BOOTROM_SIZE, 0);
    }

    if (!file.read(reinterpret_cast<char *>(is_bootrom_file ? bootrom.get() : placeholder_memory.get()), number_of_bytes_to_load))
	{
        if (is_bootrom_file)
            bootrom.reset();

        std::cerr << "Error: could not read file " << file_path << ".\n";
        return false;
    }

    return true;
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
	else if (address >= VIDEO_RAM_START && address < VIDEO_RAM_START + VIDEO_RAM_SIZE)
	{
		return pixel_processing_unit.read_byte_video_ram(address);
	}
	else if (address >= OBJECT_ATTRIBUTE_MEMORY_START && address < OBJECT_ATTRIBUTE_MEMORY_START + OBJECT_ATTRIBUTE_MEMORY_SIZE)
	{
		return pixel_processing_unit.read_byte_object_attribute_memory(address);
	}
	else
	{
		switch (address)
		{
			case 0xff04:
				return timer.read_divider_div();
			case 0xff05:
				return timer.read_timer_tima();
			case 0xff06:
				return timer.read_timer_modulo_tma();
			case 0xff07:
				return timer.read_timer_control_tac();
			case 0xff0f:
				return interrupt_flag_if | 0b11100000;
			case 0xff40:
				return pixel_processing_unit.lcd_control_lcdc;
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
			case 0xffff:
				return interrupt_enable_ie | 0b11100000;
			default:
				return placeholder_memory[address];
		}
	}
}

void MemoryManagementUnit::write_byte(uint16_t address, uint8_t value)
{
	if (address >= VIDEO_RAM_START && address < VIDEO_RAM_START + VIDEO_RAM_SIZE)
	{
		pixel_processing_unit.write_byte_video_ram(address, value);
	}
	else if (address >= OBJECT_ATTRIBUTE_MEMORY_START && address < OBJECT_ATTRIBUTE_MEMORY_START + OBJECT_ATTRIBUTE_MEMORY_SIZE)
	{
		pixel_processing_unit.write_byte_object_attribute_memory(address, value);
	}
	else
	{
		switch (address)
		{
			case 0xff04:
				timer.write_divider_div(value);
				return;
			case 0xff05:
				timer.write_timer_tima(value);
				return;
			case 0xff06:
				timer.write_timer_modulo_tma(value);
				return;
			case 0xff07:
				timer.write_timer_control_tac(value);
				return;
			case 0xff0f:
				interrupt_flag_if = value | 0b11100000;
				return;
			case 0xff40:
				pixel_processing_unit.lcd_control_lcdc = value;
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
			case 0xffff:
				interrupt_enable_ie = value | 0b11100000;
				return;
			default:
				placeholder_memory[address] = value;
				return;
		}
	}
}

void MemoryManagementUnit::request_interrupt(uint8_t interrupt_flag_mask)
{
    interrupt_flag_if |= interrupt_flag_mask;
}

bool MemoryManagementUnit::is_interrupt_type_requested(uint8_t interrupt_flag_mask) const
{
    return (interrupt_flag_if & interrupt_flag_mask) != 0;
}

bool MemoryManagementUnit::is_interrupt_type_enabled(uint8_t interrupt_flag_mask) const
{
    return (interrupt_enable_ie & interrupt_flag_mask) != 0;
}

void MemoryManagementUnit::clear_interrupt_flag_bit(uint8_t interrupt_flag_mask)
{
    interrupt_flag_if &= ~interrupt_flag_mask;
}

void MemoryManagementUnit::print_bytes_in_range(uint16_t start_address, uint16_t end_address) const
{
    std::cout << std::hex << std::setfill('0');
    std::cout << "=========== Memory Range 0x" << std::setw(4) << start_address << " - 0x" << std::setw(4) << end_address << " ============\n";

    for (uint16_t address = start_address; address <= end_address; address++)
	{
        uint16_t remainder = address % 0x10;

        if (address == start_address || remainder == 0)
		{
            uint16_t line_offset = address - remainder;
            std::cout << std::setw(4) << line_offset << ": ";

            for (uint16_t i = 0; i < remainder; i++)
                std::cout << "   ";
        }

        std::cout << std::setw(2) << static_cast<int>(read_byte(address)) << " ";

        if ((address + 1) % 0x10 == 0)
            std::cout << "\n";
    }

	if ((end_address + 1) % 0x10 != 0)
		std::cout << "\n";

	std::cout << "=====================================================\n";
}

void MemoryManagementUnit::step_single_machine_cycle()
{
    timer.step_single_machine_cycle();
}

void MemoryManagementUnit::wrote_to_read_only_address(uint16_t address) const
{
	std::cout << "Attempted to write to read only address " << address << ". No write will occur.\n";
}

} //namespace GameBoy
