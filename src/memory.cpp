#include <fstream>
#include <iostream>
#include "memory.h"

namespace GameBoy {

Memory::Memory() {
    placeholder_memory = std::make_unique<uint8_t[]>(MEMORY_SIZE);
    std::fill_n(placeholder_memory.get(), MEMORY_SIZE, 0);
}

void Memory::reset() {
    std::fill_n(placeholder_memory.get(), MEMORY_SIZE, 0);
    bootrom.reset();
}

void Memory::set_post_boot_state() {
    write_8(0xff00, 0xcf);
    write_8(0xff01, 0x00);
    write_8(0xff02, 0x7e);
    write_8(0xff04, 0xab);
    write_8(0xff05, 0x00);
    write_8(0xff06, 0x00);
    write_8(0xff07, 0xf8);
    write_8(0xff0f, 0xe1);
    write_8(0xff10, 0x80);
    write_8(0xff11, 0xbf);
    write_8(0xff12, 0xf3);
    write_8(0xff13, 0xff);
    write_8(0xff14, 0xbf);
    write_8(0xff16, 0x3f);
    write_8(0xff17, 0x00);
    write_8(0xff18, 0xff);
    write_8(0xff19, 0xbf);
    write_8(0xff1a, 0x7f);
    write_8(0xff1b, 0xff);
    write_8(0xff1c, 0x9f);
    write_8(0xff1d, 0xff);
    write_8(0xff1e, 0xbf);
    write_8(0xff20, 0xff);
    write_8(0xff21, 0x00);
    write_8(0xff22, 0x00);
    write_8(0xff23, 0xbf);
    write_8(0xff24, 0x77);
    write_8(0xff25, 0xf3);
    write_8(0xff26, 0xf1);
    write_8(0xff40, 0x90);
    write_8(0xff41, 0x85);
    write_8(0xff42, 0x00);
    write_8(0xff43, 0x00);
    write_8(0xff44, 0x00);
    write_8(0xff45, 0x00);
    write_8(0xff46, 0xff);
    write_8(0xff47, 0xfc);
    write_8(0xff48, 0x00);
    write_8(0xff49, 0x00);
    write_8(0xff4a, 0x00);
    write_8(0xff4b, 0x00);
    write_8(0xffff, 0x00);
}

uint8_t Memory::read_8(uint16_t address) const {
    bool is_bootrom_mapped = placeholder_memory[BOOTROM_STATUS_ADDRESS] == 0;

    if (is_bootrom_mapped && address < BOOTROM_SIZE) {
        if (bootrom == nullptr) {
            std::cerr << std::hex << std::setfill('0');
            std::cerr << "Warning: attempted read from address (" << std::setw(4) << address << ") pointing to an unallocated boot ROM."
                      << "Returning 0xff as a fallback.\n";
            return 0xff;
        }    
        return bootrom[address];
    }
    return address == 0xff44 ? 0x90 : placeholder_memory[address];
}

void Memory::write_8(uint16_t address, uint8_t value) {
    placeholder_memory[address] = value;
}

uint16_t Memory::read_16(uint16_t address) const {
    // Little-endian read, first byte is least significant
    if (address == 0xffff) {
        std::cerr << "Warning: Attempted 16-bit read at 0xffff which would access out-of-bounds memory. Returning only the lower byte.\n";
        return read_8(address);
    }
    return (read_8(address + 1) << 8) | read_8(address);
}

void Memory::write_16(uint16_t address, uint16_t value) {
    // Little-endian write, first byte is least significant
    write_8(address, value & 0xff);
    if (address != 0xffff) {
        write_8(address + 1, value >> 8);
    } 
    else {
        std::cerr << "Warning: Attempted 16-bit write at 0xffff which would access out-of-bounds memory. Wrote only the lower byte.\n";
    }
}

void Memory::print_bytes_in_range(uint16_t start_address, uint16_t end_address) const {
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

        std::cout << std::setw(2) << static_cast<int>(read_8(address)) << " ";

        if ((address + 1) % 0x10 == 0) {
            std::cout << "\n";
        }
    }

    if ((end_address + 1) % 0x10 != 0) {
        std::cout << "\n";
    }
    std::cout << "=====================================================\n";
}

bool Memory::try_load_file(uint16_t address, uint32_t number_of_bytes_to_load, std::filesystem::path file_path, bool is_bootrom_file) {
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

    if (!file.read(reinterpret_cast<char*>(is_bootrom_file ? bootrom.get() : placeholder_memory.get()), number_of_bytes_to_load)) {
        if (is_bootrom_file) {
            bootrom.reset();
        }
        std::cerr << "Error: could not read file " << file_path << ".\n";
        return false;
    }

    return true;
}

} //namespace GameBoy
