#pragma once

#include <cstdint>
#include "memory.h"

namespace GameBoy {
    constexpr std::uint8_t FLAG_Z = 1 << 7; // Zero - set after arithmetic result is zero, cleared when result is nonzero
    constexpr std::uint8_t FLAG_N = 1 << 6; // Subtract - set after subtraction/decrement/compare, cleared after addition/increment/logical operation (and LD HL, SP + e8)
    constexpr std::uint8_t FLAG_H = 1 << 5; // Half-Carry (from bit 3-4, or sometimes from 11 to 12) - set when addition causes carry or subtraction requires borrow, cleared otherwise
    constexpr std::uint8_t FLAG_C = 1 << 4; // Carry (from bit 7-8, or sometimes from 15-16) - set when addition causes carry, subtraction requires borrow, or bit shifted out, cleared otherwise

    class CPU {
    public:
        CPU(Memory mem) : memory(mem) {}
        void execute_instruction(std::uint8_t opcode);
        void print_internal_values() const;

    private:
        Memory memory;
        std::uint64_t cycles_elapsed{};

        // 8-bit registers can be accessed individually or together through their corresponding 16-bit register pair
        // The first letter of each register pair is its most significant byte and the second is its least significant byte
        union {
            struct {
                std::uint8_t a;
                std::uint8_t f; // Flags register - only bits 7-4 are used (3-0 will always be zero)
            };
            std::uint16_t af{};
        };
        union {
            struct {
                std::uint8_t b;
                std::uint8_t c;
            };
            std::uint16_t bc{};
        };
        union {
            struct {
                std::uint8_t d;
                std::uint8_t e;
            };
            std::uint16_t de{};
        };
        union {
            struct {
                std::uint8_t h;
                std::uint8_t l;
            };
            std::uint16_t hl{};
        };
        std::uint16_t sp{}; // Stack Pointer, address of the top of the stack in WRAM
        std::uint16_t pc{}; // Program Counter, address of the next instruction to execute

        void nop_0x00();
        void ld_bc_imm_0x01();
        void ld_mem_bc_a_0x02();
        void inc_bc_0x03();
        void inc_b_0x04();
        void dec_b_0x05();
    };
}
