#include <algorithm>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <sstream>

#include "bitwise_utilities.h"
#include "console_output_utilities.h"
#include "memory_management_unit.h"

namespace GameBoyCore
{

MemoryManagementUnit::MemoryManagementUnit(GameCartridgeSlot &game_cartridge_slot_reference, InternalTimer & internal_timer_reference, PixelProcessingUnit &pixel_processing_unit_reference)
    : game_cartridge_slot{game_cartridge_slot_reference},
      internal_timer{internal_timer_reference},
      pixel_processing_unit{pixel_processing_unit_reference}
{
    bootrom = std::make_unique<uint8_t[]>(BOOTROM_SIZE);
    work_ram = std::make_unique<uint8_t[]>(WORK_RAM_SIZE);
    unmapped_input_output_registers = std::make_unique<uint8_t[]>(INPUT_OUTPUT_REGISTERS_SIZE);
    high_ram = std::make_unique<uint8_t[]>(HIGH_RAM_SIZE);

    std::fill_n(bootrom.get(), BOOTROM_SIZE, 0);
    std::fill_n(work_ram.get(), WORK_RAM_SIZE, 0);
    std::fill_n(unmapped_input_output_registers.get(), INPUT_OUTPUT_REGISTERS_SIZE, 0);
    std::fill_n(high_ram.get(), HIGH_RAM_SIZE, 0);
}

void MemoryManagementUnit::reset_state()
{
    std::fill_n(work_ram.get(), WORK_RAM_SIZE, 0);
    std::fill_n(unmapped_input_output_registers.get(), INPUT_OUTPUT_REGISTERS_SIZE, 0);
    std::fill_n(high_ram.get(), HIGH_RAM_SIZE, 0);

    joypad_p1_joyp = 0b11111111;
    interrupt_flag_if = 0b11100000;
    bootrom_status = 0x00;
    interrupt_enable_ie = 0b00000000;

    oam_dma_startup_state = ObjectAttributeMemoryDirectMemoryAccessStartupState::NotStarting;
    oam_dma_source_address_base = 0x0000;
    oam_dma_machine_cycles_elapsed = 0;
}

void MemoryManagementUnit::set_post_boot_state()
{
    bootrom_status = 0x01;
    joypad_p1_joyp = 0b11001111;
    write_byte(0xff01, 0x00, false);
    write_byte(0xff02, 0x7e, false);
    interrupt_flag_if = 0b11100001;
    write_byte(0xff10, 0x80, false);
    write_byte(0xff11, 0xbf, false);
    write_byte(0xff12, 0xf3, false);
    write_byte(0xff13, 0xff, false);
    write_byte(0xff14, 0xbf, false);
    write_byte(0xff16, 0x3f, false);
    write_byte(0xff17, 0x00, false);
    write_byte(0xff18, 0xff, false);
    write_byte(0xff19, 0xbf, false);
    write_byte(0xff1a, 0x7f, false);
    write_byte(0xff1b, 0xff, false);
    write_byte(0xff1c, 0x9f, false);
    write_byte(0xff1d, 0xff, false);
    write_byte(0xff1e, 0xbf, false);
    write_byte(0xff20, 0xff, false);
    write_byte(0xff21, 0x00, false);
    write_byte(0xff22, 0x00, false);
    write_byte(0xff23, 0xbf, false);
    write_byte(0xff24, 0x77, false);
    write_byte(0xff25, 0xf3, false);
    write_byte(0xff26, 0xf1, false);
    interrupt_enable_ie = 0b00000000;

    oam_dma_startup_state = ObjectAttributeMemoryDirectMemoryAccessStartupState::NotStarting;
    oam_dma_source_address_base = 0x0000;
    oam_dma_machine_cycles_elapsed = 0;
}

bool MemoryManagementUnit::try_load_file(const std::filesystem::path &file_path, bool is_bootrom_file, std::string &error_message)
{
    std::ifstream file(file_path, std::ios::binary | std::ios::ate);
    if (!file)
    {
        return set_error_message_and_fail(std::string("File not found at ") + file_path.string(), error_message);
    }
    std::streamsize file_length_in_bytes = file.tellg();
    file.seekg(0, std::ios::beg);

    if (is_bootrom_file)
    {
        if (file_length_in_bytes != BOOTROM_SIZE)
        {
            return set_error_message_and_fail(std::string("Provided file of size ") + std::to_string(file_length_in_bytes) +
                                              std::string(" bytes does not meet the boot ROM size requirement."), error_message);
        }

        if (!file.read(reinterpret_cast<char *>(bootrom.get()), file_length_in_bytes))
        {
            return set_error_message_and_fail(std::string("Could not read bootrom file ") + file_path.string(), error_message);
        }
        is_bootrom_loaded_in_memory.store(true, std::memory_order_release);
    }
    else
    {
        if (!game_cartridge_slot.try_load_file(file_path, file, file_length_in_bytes, error_message))
        {
            return false;
        }
        is_game_rom_loaded_in_memory.store(true, std::memory_order_release);
    }
    return true;
}

void MemoryManagementUnit::unload_bootrom_thread_safe()
{
    std::fill_n(bootrom.get(), BOOTROM_SIZE, 0);
    is_bootrom_loaded_in_memory.store(false, std::memory_order_release);
}

void MemoryManagementUnit::unload_game_rom_thread_safe()
{
    game_cartridge_slot.reset_state();
    is_game_rom_loaded_in_memory.store(false, std::memory_order_release);
}

bool MemoryManagementUnit::is_bootrom_loaded_thread_safe() const
{
    return is_bootrom_loaded_in_memory.load(std::memory_order_acquire);
}

bool MemoryManagementUnit::is_game_rom_loaded_thread_safe() const
{
    return is_game_rom_loaded_in_memory.load(std::memory_order_acquire);
}

uint8_t MemoryManagementUnit::read_byte(uint16_t address, bool is_access_for_oam_dma) const
{
    if (pixel_processing_unit.is_oam_dma_in_progress && !is_access_for_oam_dma && are_addresses_on_same_bus(address, oam_dma_source_address_base))
    {
        const uint16_t oam_dma_byte_to_transfer_address = oam_dma_source_address_base + oam_dma_machine_cycles_elapsed;
        address = oam_dma_byte_to_transfer_address;
    }

    if (bootrom_status == 0 && address < BOOTROM_SIZE)
    {
        return bootrom[address];
    }
    else if (address < ROM_BANK_0X_START + ROM_BANK_SIZE)
    {
        return game_cartridge_slot.read_byte(address);
    }
    else if (address < VIDEO_RAM_START + VIDEO_RAM_SIZE)
    {
        return pixel_processing_unit.read_byte_video_ram(address);
    }
    else if (address < EXTERNAL_RAM_START + EXTERNAL_RAM_SIZE)
    {
        return game_cartridge_slot.read_byte(address);
    }
    else if (address < WORK_RAM_START + WORK_RAM_SIZE)
    {
        const uint16_t local_address = address - WORK_RAM_START;
        return work_ram[local_address];
    }
    else if (address < ECHO_RAM_START + ECHO_RAM_SIZE)
    {
        const uint16_t local_address = address - ECHO_RAM_START;
        return work_ram[local_address];
    }
    else if (address < OBJECT_ATTRIBUTE_MEMORY_START + OBJECT_ATTRIBUTE_MEMORY_SIZE)
    {
        return pixel_processing_unit.read_byte_object_attribute_memory(address, true);
    }
    else if (address < UNUSABLE_MEMORY_START + UNUSABLE_MEMORY_SIZE)
    {
        return 0x00;
    }
    else if (address < INPUT_OUTPUT_REGISTERS_START + INPUT_OUTPUT_REGISTERS_SIZE)
    {
        switch (address)
        {
            case 0xff00:
            {
                const bool is_select_buttons_enabled = !is_bit_set(joypad_p1_joyp, 5);
                const bool is_select_directional_pad_enabled = !is_bit_set(joypad_p1_joyp, 4);

                if (is_select_buttons_enabled && is_select_directional_pad_enabled)
                {
                    return (joypad_p1_joyp & 0xf0) | ((joypad_button_states.load(std::memory_order_acquire) |
                                                       joypad_direction_pad_states.load(std::memory_order_acquire)) & 0x0f);
                }
                if (is_select_buttons_enabled)
                {
                    return (joypad_p1_joyp & 0xf0) | (joypad_button_states.load(std::memory_order_acquire) & 0x0f);
                }
                else if (is_select_directional_pad_enabled)
                {
                    return (joypad_p1_joyp & 0xf0) | (joypad_direction_pad_states.load(std::memory_order_acquire) & 0x0f);
                }
                return joypad_p1_joyp;
            }
            case 0xff04:
                return internal_timer.read_div();
            case 0xff05:
                return internal_timer.read_tima();
            case 0xff06:
                return internal_timer.read_tma();
            case 0xff07:
                return internal_timer.read_tac();
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
    else if (address < HIGH_RAM_START + HIGH_RAM_SIZE)
    {
        const uint16_t local_address = address - HIGH_RAM_START;
        return high_ram[local_address];
    }
    else
        return interrupt_enable_ie;
}

void MemoryManagementUnit::write_byte(uint16_t address, uint8_t value, bool is_access_for_oam_dma)
{
    if (address < ROM_BANK_0X_START + ROM_BANK_SIZE)
    {
        game_cartridge_slot.write_byte(address, value);
    }
    else if (address < VIDEO_RAM_START + VIDEO_RAM_SIZE)
    {
        pixel_processing_unit.write_byte_video_ram(address, value);
    }
    else if (address < EXTERNAL_RAM_START + EXTERNAL_RAM_SIZE)
    {
        game_cartridge_slot.write_byte(address, value);
    }
    else if (address < WORK_RAM_START + WORK_RAM_SIZE)
    {
        const uint16_t local_address = address - WORK_RAM_START;
        work_ram[local_address] = value;
    }
    else if (address < ECHO_RAM_START + ECHO_RAM_SIZE)
    {
        const uint16_t local_address = address - ECHO_RAM_START;
        work_ram[local_address] = value;
    }
    else if (address < OBJECT_ATTRIBUTE_MEMORY_START + OBJECT_ATTRIBUTE_MEMORY_SIZE)
    {
        pixel_processing_unit.write_byte_object_attribute_memory(address, value, is_access_for_oam_dma);
    }
    else if (address < UNUSABLE_MEMORY_START + UNUSABLE_MEMORY_SIZE)
    {
        std::cout << std::hex << std::setfill('0') << "Attempted to write to unusable address 0x" << std::setw(4) << address << ". No write will occur.\n";
    }
    else if (address < INPUT_OUTPUT_REGISTERS_START + INPUT_OUTPUT_REGISTERS_SIZE)
    {
        switch (address)
        {
            case 0xff00:
                joypad_p1_joyp = value | 0b11001111;
                return;
            case 0xff04:
                internal_timer.write_div(value);
                return;
            case 0xff05:
                internal_timer.write_tima(value);
                return;
            case 0xff06:
                internal_timer.write_tma(value);
                return;
            case 0xff07:
                internal_timer.write_tac(value);
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
                std::cout << std::hex << std::setfill('0') << "Attempted to write to read only address 0x" << std::setw(4) << address << ". No write will occur.\n";
                return;
            case 0xff45:
                pixel_processing_unit.lcd_y_coordinate_compare_lyc = value;
                return;
            case 0xff46:
                pixel_processing_unit.object_attribute_memory_direct_memory_access_dma = value;
                oam_dma_startup_state = ObjectAttributeMemoryDirectMemoryAccessStartupState::RegisterWrittenTo;
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
    else if (address < HIGH_RAM_START + HIGH_RAM_SIZE)
    {
        const uint16_t local_address = address - HIGH_RAM_START;
        high_ram[local_address] = value;
    }
    else
        interrupt_enable_ie = value;
}

void MemoryManagementUnit::step_single_machine_cycle()
{
    if (pixel_processing_unit.is_oam_dma_in_progress)
    {
        const uint16_t source_address = oam_dma_source_address_base + oam_dma_machine_cycles_elapsed;
        const uint8_t byte_to_copy = read_byte(source_address, true);

        const uint16_t destination_address = OBJECT_ATTRIBUTE_MEMORY_START + oam_dma_machine_cycles_elapsed;
        write_byte(destination_address, byte_to_copy, true);

        if (++oam_dma_machine_cycles_elapsed == OAM_DMA_MACHINE_CYCLE_DURATION)
        {
            pixel_processing_unit.is_oam_dma_in_progress = false;
        }
    }

    if (oam_dma_startup_state == ObjectAttributeMemoryDirectMemoryAccessStartupState::RegisterWrittenTo)
    {
        oam_dma_startup_state = ObjectAttributeMemoryDirectMemoryAccessStartupState::Starting;
    }
    else if (oam_dma_startup_state == ObjectAttributeMemoryDirectMemoryAccessStartupState::Starting)
    {
        oam_dma_source_address_base = ((pixel_processing_unit.object_attribute_memory_direct_memory_access_dma >= 0xfe)
            ? pixel_processing_unit.object_attribute_memory_direct_memory_access_dma - 0x20
            : pixel_processing_unit.object_attribute_memory_direct_memory_access_dma) << 8;

        oam_dma_machine_cycles_elapsed = 0;
        pixel_processing_unit.is_oam_dma_in_progress = true;
        oam_dma_startup_state = ObjectAttributeMemoryDirectMemoryAccessStartupState::NotStarting;
    }
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
        const uint8_t interrupt_flag_mask = 1 << i;
        const bool is_interrupt_type_requested = is_flag_set(interrupt_flag_if, interrupt_flag_mask);
        const bool is_interrupt_type_enabled = is_flag_set(interrupt_enable_ie, interrupt_flag_mask);

        if (is_interrupt_type_requested && is_interrupt_type_enabled)
            return interrupt_flag_mask;
    }
    return 0x00;
}

void MemoryManagementUnit::update_joypad_button_pressed_state_thread_safe(uint8_t button_flag_mask, bool new_button_pressed_state)
{
    if (new_button_pressed_state)
        joypad_button_states.fetch_or(button_flag_mask, std::memory_order_release);
    else
        joypad_button_states.fetch_and(~button_flag_mask, std::memory_order_release);
}

void MemoryManagementUnit::update_joypad_direction_pad_pressed_state_thread_safe(uint8_t direction_flag_mask, bool new_direction_pressed_state)
{
    if (new_direction_pressed_state)
        joypad_direction_pad_states.fetch_or(direction_flag_mask, std::memory_order_release);
    else
        joypad_direction_pad_states.fetch_and(~direction_flag_mask, std::memory_order_release);
}

bool MemoryManagementUnit::are_addresses_on_same_bus(uint16_t first_address, uint16_t second_address) const
{
    static constexpr std::array<std::pair<uint16_t, uint16_t>, 6> memory_buses
    {
        {
            {ROM_BANK_X0_START, ROM_BANK_SIZE},
            {ROM_BANK_0X_START, ROM_BANK_SIZE},
            {VIDEO_RAM_START, VIDEO_RAM_SIZE},
            {EXTERNAL_RAM_START, EXTERNAL_RAM_SIZE},
            {WORK_RAM_START, WORK_RAM_SIZE},
            {ECHO_RAM_START, ECHO_RAM_SIZE},
        }
    };

    auto in_range = [](uint16_t address, uint16_t range_start, uint16_t range_size)
    {
        return address >= range_start && address < range_start + range_size;
    };

    for (auto &[range_start, range_size] : memory_buses)
    {
        if (in_range(first_address, range_start, range_size) && in_range(second_address, range_start, range_size))
        {
            return true;
        }
    }
    return false;
}

} // namespace GameBoyCore
