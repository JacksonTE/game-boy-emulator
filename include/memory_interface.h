#pragma once

#include <filesystem>
#include <fstream>
#include <iostream>

namespace GameBoy {

constexpr uint32_t MEMORY_SIZE = 0x10000;
constexpr uint16_t COLLECTIVE_ROM_BANK_SIZE = 0x8000;
constexpr uint16_t BOOTROM_SIZE = 0x100;

constexpr uint16_t HIGH_RAM_START = 0xff00;
constexpr uint16_t BOOTROM_STATUS_ADDRESS = 0xff50;

class MemoryInterface {
public:
    virtual void reset_state() = 0;
    virtual void set_post_boot_state() = 0;
    virtual bool try_load_file(uint16_t address, uint32_t number_of_bytes_to_load, std::filesystem::path file_path, bool is_bootrom_file) = 0;

    virtual uint8_t read_8(uint16_t address) const = 0;
    virtual void write_8(uint16_t address, uint8_t value) = 0;

    virtual uint16_t read_16(uint16_t address) const {
        // Little-endian read, first byte is least significant
        if (address == 0xffff) {
            std::cerr << "Warning: Attempted 16-bit read at 0xffff which would access out-of-bounds memory. Returning only the lower byte.\n";
            return read_8(address); // TODO add & read_8(address) when switching to per m-cycle ticking?
        }
        return (read_8(address + 1) << 8) | read_8(address);
    }

    virtual void write_16(uint16_t address, uint16_t value) {
        // Little-endian write, first byte is least significant
        write_8(address, value & 0xff); // TODO double write when switching to per m-cycle ticking?
        if (address != 0xffff) {
            write_8(address + 1, value >> 8);
        }
        else {
            std::cerr << "Warning: Attempted 16-bit write at 0xffff which would access out-of-bounds memory. Wrote only the lower byte.\n";
        }
    }

    virtual void print_bytes_in_range(uint16_t start_address, uint16_t end_address) const {
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
};

} // namespace GameBoy
