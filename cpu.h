#pragma once

#include <cstdint>

namespace GameBoy {
    class CPU {
    private:
        // Game Boy is little-endian - operations with combination registers expect lower byte first in memory
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

        std::uint64_t cyclesElapsed{};

        void nop_00();
        void ld_01();

    public:
        void executeInstruction(std::uint8_t opcode);
    };
}
