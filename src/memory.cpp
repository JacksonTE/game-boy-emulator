#include <fstream>
#include <iostream>
#include "memory.h"

namespace GameBoy {

void Memory::print_range(uint16_t from_address, uint16_t to_address) const {
    std::cout << std::hex << std::setfill('0');
    std::cout << "============= Memory Range - " << std::setw(4) << from_address << ":" << std::setw(4) << to_address << " ==============\n";
    for (uint16_t address = from_address; address <= to_address; address++) {
        if (address % 16 == 0) {
            std::cout << std::setw(4) << address << ": ";
        }

        std::cout << std::setw(2) << static_cast<unsigned>(placeholder_memory[address]) << " ";

        if ((address + 1) % 16 == 0) {
            std::cout << "\n";
        }
    }

    if ((to_address - from_address + 1) % 16 != 0) {
        std::cout << "\n";
    }
}

uint8_t Memory::read_8(uint16_t address) const {
    return placeholder_memory[address];
}

void Memory::write_8(uint16_t address, uint8_t value) {
    placeholder_memory[address] = value;
}

uint16_t Memory::read_16(uint16_t address) const {
    // Little-endian read, first byte is least significant
    if (address == 0xffff) {
        std::cerr << "Warning: Attempted 16-bit read at 0xffff. Returning only the lower byte.\n";
        return placeholder_memory[address];
    }
    return (placeholder_memory[address + 1] << 8) | placeholder_memory[address];
}

void Memory::write_16(uint16_t address, uint16_t value) {
    // Little-endian write, first byte is least significant
    placeholder_memory[address] = value & 0xff;
    if (address != 0xffff) {
        placeholder_memory[address + 1] = value >> 8;
    } else {
        std::cerr << "Warning: Attempted 16-bit write at 0xffff. Wrote only the lower byte.\n";
    }
}

bool Memory::try_write_range_from_file(uint16_t address, uint32_t number_of_bytes, std::filesystem::path file_path) {
    std::ifstream file(file_path, std::ios::binary | std::ios::ate);
    if (!file) {
        std::cerr << "Error: file not found at " << file_path << ".\n";
        return false;
    }

    std::streamsize file_length_in_bytes = file.tellg();
    if (file_length_in_bytes < static_cast<std::streamsize>(number_of_bytes)) {
        std::cerr << std::hex;
        std::cerr << "Error: file size (" << file_length_in_bytes << ") is less than requested number of bytes to load (" << number_of_bytes << ").\n";
        return false;
    }

    if (address + number_of_bytes > MEMORY_SIZE) {
        std::cerr << std::hex << std::setfill('0');
        std::cerr << "Error: insufficient space from starting address (" << std::setw(4) << address
                  << ") to load requested number of bytes (" << number_of_bytes << ").\n";
        return false;
    }

    file.seekg(0, std::ios::beg);
    if (!file.read(reinterpret_cast<char*>(&placeholder_memory[address]), number_of_bytes)) {
        std::cerr << "Error: could not read file " << file_path << ".\n";
        return false;
    }

    return true;
}

} //namespace GameBoy
