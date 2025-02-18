#pragma once

#include <cstdint>

namespace GameBoy {
    class CPU {
    private:
        // Game Boy is little-endian - lower byte stored first in memory for these combination registers
        union {
            struct {
                std::uint8_t f;
                std::uint8_t a;
            };
            std::uint16_t af{};
        };
        union {
            struct {
                std::uint8_t c;
                std::uint8_t b;
            };
            std::uint16_t bc{};
        };
        union {
            struct {
                std::uint8_t e;
                std::uint8_t d;
            };
            std::uint16_t de{};
        };
        union {
            struct {
                std::uint8_t l;
                std::uint8_t h;
            };
            std::uint16_t hl{};
        };
        std::uint16_t sp{};
        std::uint16_t pc{};

        using InstructionFunc = void(*)(CPU&);
        static const InstructionFunc instructionTable[256];

        static void nop(CPU& cpu);

    public:
        void executeInstruction(std::uint8_t opcode);
    };
}
