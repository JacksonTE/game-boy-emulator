#pragma once

#include "memory.h"
#include <cstdint>

namespace GameBoy {
    class CPU {
    public:
        CPU(Memory mem) : memory(mem) {}
        void executeInstruction(std::uint8_t opcode);

    private:
        Memory memory;
        std::uint64_t cyclesElapsed{};

        // 8-bit registers can be accessed individually or together through their corresponding 16-bit register pair
        // The first letter of each register pair is its MSB, and the second letter is its LSB
        union {
            struct {
                std::uint8_t a;
                std::uint8_t f;
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
        std::uint16_t sp{};
        std::uint16_t pc{};

        void nop00();
        void ld01();
    };
}
