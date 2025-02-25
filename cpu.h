#pragma once

#include <cstdint>

namespace GameBoy {
    class CPU {
    public:
        void executeInstruction(std::uint8_t opcode);

    private:
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

        std::uint64_t cyclesElapsed{};

        void nop00();
        void ld01();
    };
}
