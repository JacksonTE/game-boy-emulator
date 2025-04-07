#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include "memory_management_unit.h"

namespace GameBoy {

MemoryManagementUnit::MemoryManagementUnit() {
    placeholder_memory = std::make_unique<uint8_t[]>(MEMORY_SIZE);
    std::fill_n(placeholder_memory.get(), MEMORY_SIZE, 0);
}

void MemoryManagementUnit::reset_state() {
    std::fill_n(placeholder_memory.get(), MEMORY_SIZE, 0);
    bootrom.reset();
}

void MemoryManagementUnit::set_post_boot_state() {
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

bool MemoryManagementUnit::try_load_file(uint16_t address, uint32_t number_of_bytes_to_load, std::filesystem::path file_path, bool is_bootrom_file) {
    std::ifstream file(file_path, std::ios::binary | std::ios::ate);
    if (!file) {
        std::cerr << "Error: file not found at " << file_path << ".\n";
        return false;
    }

    std::streamsize file_length_in_bytes = file.tellg();
    if (file_length_in_bytes < static_cast<std::streamsize>(number_of_bytes_to_load)) {
        std::cerr << std::hex;
        std::cerr << "Error: file size (" << file_length_in_bytes << ") is less than requested number of bytes to load (" << number_of_bytes_to_load << ").\n";
        return false;
    }

    if (address + number_of_bytes_to_load > (is_bootrom_file ? BOOTROM_SIZE : COLLECTIVE_ROM_BANK_SIZE)) {
        std::cerr << std::hex << std::setfill('0');
        std::cerr << "Error: insufficient space from starting address (" << std::setw(4) << address
            << ") to load requested number of bytes (" << number_of_bytes_to_load << ").\n";
        return false;
    }

    file.seekg(0, std::ios::beg);

    if (is_bootrom_file) {
        if (bootrom == nullptr) {
            bootrom = std::make_unique<uint8_t[]>(BOOTROM_SIZE);
        }
        std::fill_n(bootrom.get(), BOOTROM_SIZE, 0);
    }

    if (!file.read(reinterpret_cast<char *>(is_bootrom_file ? bootrom.get() : placeholder_memory.get()), number_of_bytes_to_load)) {
        if (is_bootrom_file) {
            bootrom.reset();
        }
        std::cerr << "Error: could not read file " << file_path << ".\n";
        return false;
    }

    return true;
}

uint8_t MemoryManagementUnit::read_byte(uint16_t address) const {
    if (bootrom_status == 0 && address < BOOTROM_SIZE) {
        if (bootrom == nullptr) {
            std::cerr << std::hex << std::setfill('0');
            std::cerr << "Warning: attempted read from bootrom address (" << std::setw(4) << address << ") pointing to an unallocated bootrom. "
                      << "Returning 0xff as a fallback.\n";
            return 0xff;
        }
        return bootrom[address];
    }
    else if (address == 0xff04) {
        return get_divider_div();
    }
    else if (address == 0xff05) {
        return timer_tima;
    }
    else if (address == 0xff06) {
        return timer_modulo_tma;
    }
    else if (address == 0xff07) {
        return timer_control_tac;
    }
    else if (address == 0xff0f) {
        return interrupt_flag_if | 0b11100000;
    }
    else if (address == 0xff44) {
        return 0x90;
    }
    else if (address == 0xff50) {
        return bootrom_status;
    }
    else if (address == 0xffff) {
        return interrupt_enable_ie | 0b11100000;
    }

    return placeholder_memory[address];
}

void MemoryManagementUnit::write_byte(uint16_t address, uint8_t value) {
    if (address == 0xff04) {
        system_counter = 0x0000;

        if (does_timer_increment_and_overflow()) {
            request_interrupt(INTERRUPT_FLAG_TIMER_MASK);
            timer_tima = timer_modulo_tma;
        }
    }
    else if (address == 0xff05) {
        if (is_timer_tima_overflow_handled) {
            return;
        }
        timer_tima = value;
        did_timer_tima_overflow = false;
    }
    else if (address == 0xff06) {
        timer_modulo_tma = value;

        if (is_timer_tima_overflow_handled) {
            timer_tima = timer_modulo_tma;
        }
    }
    else if (address == 0xff07) {
        timer_control_tac = value;

        if (does_timer_increment_and_overflow()) {
            request_interrupt(INTERRUPT_FLAG_TIMER_MASK);
            timer_tima = timer_modulo_tma;
        }
    }
    else if (address == 0xff0f) {
        interrupt_flag_if = value | 0b11100000;
    }
    else if (address == 0xff50) {
        bootrom_status = value;
    }
    else if (address == 0xffff) {
        interrupt_enable_ie = value | 0b11100000;
    }
    else {
        placeholder_memory[address] = value;
    }
}

void MemoryManagementUnit::print_bytes_in_range(uint16_t start_address, uint16_t end_address) const {
    std::cout << std::hex << std::setfill('0');
    std::cout << "=========== Memory Range 0x" << std::setw(4) << start_address << " - 0x" << std::setw(4) << end_address << " ============\n";

    for (uint16_t address = start_address; address <= end_address; address++) {
        uint16_t remainder = address % 0x10;

        if (address == start_address || remainder == 0) {
            uint16_t line_offset = address - remainder;
            std::cout << std::setw(4) << line_offset << ": ";

            for (uint16_t i = 0; i < remainder; i++) {
                std::cout << "   ";
            }
        }

        std::cout << std::setw(2) << static_cast<int>(read_byte(address)) << " ";

        if ((address + 1) % 0x10 == 0) {
            std::cout << "\n";
        }
    }

    if ((end_address + 1) % 0x10 != 0) {
        std::cout << "\n";
    }
    std::cout << "=====================================================\n";
}

void MemoryManagementUnit::request_interrupt(uint8_t interrupt_flag_mask) {
    interrupt_flag_if |= interrupt_flag_mask;
}

bool MemoryManagementUnit::is_interrupt_type_requested(uint8_t interrupt_flag_mask) const {
    return (interrupt_flag_if & interrupt_flag_mask) != 0;
}

bool MemoryManagementUnit::is_interrupt_type_enabled(uint8_t interrupt_flag_mask) const {
    return (interrupt_enable_ie & interrupt_flag_mask) != 0;
}

void MemoryManagementUnit::clear_interrupt_flag_bit(uint8_t interrupt_flag_mask) {
    interrupt_flag_if &= ~interrupt_flag_mask;
}

void MemoryManagementUnit::tick_machine_cycle() {
    system_counter += 4;

    is_timer_tima_overflow_handled = did_timer_tima_overflow;
    if (did_timer_tima_overflow) {
        request_interrupt(INTERRUPT_FLAG_TIMER_MASK);
        timer_tima = timer_modulo_tma;
        did_timer_tima_overflow = false;
    }

    did_timer_tima_overflow = does_timer_increment_and_overflow();
}

bool MemoryManagementUnit::does_timer_increment_and_overflow() {
    const bool is_timer_tima_enabled = (timer_control_tac & 0b00000100) != 0;

    const uint8_t clock_select = timer_control_tac & 0b00000011;
    const uint8_t clock_select_to_selected_system_counter_bit[4] = {9, 3, 5, 7};
    const uint8_t selected_system_counter_bit = clock_select_to_selected_system_counter_bit[clock_select];
    const bool is_selected_system_counter_bit_set = is_timer_tima_enabled &&
                                                    (system_counter & (1 << selected_system_counter_bit)) != 0;

    const bool overflow = !is_selected_system_counter_bit_set &&
                           is_previously_selected_system_counter_bit_set &&
                           (++timer_tima == 0);

    is_previously_selected_system_counter_bit_set = is_selected_system_counter_bit_set;
    return overflow;
}

uint8_t MemoryManagementUnit::get_divider_div() const {
    return static_cast<uint8_t>(system_counter >> 8);
}

} //namespace GameBoy
