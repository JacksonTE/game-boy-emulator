#pragma once

#include <cstdint>

namespace GameBoy {
    struct Registers {
        // Game Boy is little-endian - lower byte stored first in memory for these combination registers
        union {
            struct {
                std::uint8_t F;
                std::uint8_t A;
            };
            std::uint16_t AF{};
        };
        union {
            struct {
                std::uint8_t C;
                std::uint8_t B;
            };
            std::uint16_t BC{};
        };
        union {
            struct {
                std::uint8_t E;
                std::uint8_t D;
            };
            std::uint16_t DE{};
        };
        union {
            struct {
                std::uint8_t L;
                std::uint8_t H;
            };
            std::uint16_t HL{};
        };
        std::uint16_t SP{};
        std::uint16_t PC{};
    };
}
