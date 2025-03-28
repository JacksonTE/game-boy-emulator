#include <fstream>
#include <iostream>
#include "memory.h"

namespace GameBoy {

Memory::Memory() {
    placeholder_memory = std::make_unique<uint8_t[]>(MEMORY_SIZE);
    std::fill_n(placeholder_memory.get(), MEMORY_SIZE, 0);
}

void Memory::allocate_boot_rom() {
    boot_rom = std::make_unique<uint8_t[]>(BOOT_ROM_SIZE);
    std::fill_n(boot_rom.get(), BOOT_ROM_SIZE, 0);
}

void Memory::deallocate_boot_rom() {
    boot_rom.reset();
}

uint8_t Memory::read_8(uint16_t address) const {
    if (is_boot_rom_mapped() && address < BOOT_ROM_SIZE) {
        if (boot_rom == nullptr) {
            std::cerr << std::hex << std::setfill('0');
            std::cerr << "Error: attempted read from address (" << std::setw(4) << address << ") pointing to an unallocated boot ROM."
                      << "Returning 0xff as a fallback.\n";
            return 0xff;
        }    
        return boot_rom[address];
    }
    return address == 0xff44 ? 0x90 : placeholder_memory[address]; // TODO remove hardcoding - also 0x91 according to pandocs
}

void Memory::write_8(uint16_t address, uint8_t value) {
    placeholder_memory[address] = value;
}

uint16_t Memory::read_16(uint16_t address) const {
    // Little-endian read, first byte is least significant
    if (address == 0xffff) {
        std::cerr << "Warning: Attempted 16-bit read at 0xffff. Returning only the lower byte.\n";
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
        std::cerr << "Warning: Attempted 16-bit write at 0xffff. Wrote only the lower byte.\n";
    }
}

void Memory::print_bytes_in_range(uint16_t start_address, uint16_t end_address) const {
    std::cout << std::hex << std::setfill('0');
    std::cout << "=========== Memory Range 0x" << std::setw(4) << start_address << " - 0x" << std::setw(4) << end_address << " ============\n";

    for (uint16_t address = start_address; address <= end_address; address++) {
        uint16_t remainder = address % 16;

        if (address == start_address || remainder == 0) {
            uint16_t line_offset = address - remainder;
            std::cout << std::setw(4) << line_offset << ": ";

            for (uint16_t i = 0; i < remainder; i++) {
                std::cout << "   ";
            }
        }

        std::cout << std::setw(2) << static_cast<int>(read_8(address)) << " ";

        if ((address + 1) % 16 == 0) {
            std::cout << "\n";
        }
    }

    if ((end_address + 1) % 16 != 0) {
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

    if (address + number_of_bytes_to_load > (is_bootrom_file ? BOOT_ROM_SIZE : COLLECTIVE_ROM_BANK_SIZE)) {
        std::cerr << std::hex << std::setfill('0');
        std::cerr << "Error: insufficient space from starting address (" << std::setw(4) << address
                  << ") to load requested number of bytes (" << number_of_bytes_to_load << ").\n";
        return false;
    }

    if (is_bootrom_file && boot_rom == nullptr) {
        std::cerr << "Error: attempted to load to an unallocated boot ROM pointer.\n";
        return false;
    }

    file.seekg(0, std::ios::beg);

    if (!file.read(reinterpret_cast<char*>(is_bootrom_file ? boot_rom.get() : placeholder_memory.get()), number_of_bytes_to_load)) {
        std::cerr << "Error: could not read file " << file_path << ".\n";
        return false;
    }

    return true;
}

bool Memory::is_boot_rom_mapped() const {
    return placeholder_memory[BOOTROM_STATUS_ADDRESS] == 0;
}

} //namespace GameBoy
