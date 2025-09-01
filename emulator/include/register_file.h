#pragma once

#include <bit>
#include <cstdint>

constexpr uint8_t ZERO_FLAG_MASK = 1 << 7;
constexpr uint8_t SUBTRACT_FLAG_MASK = 1 << 6; // Also known as the 'N' flag
constexpr uint8_t HALF_CARRY_FLAG_MASK = 1 << 5; // For a carry from bit 3-4 or 11-12
constexpr uint8_t CARRY_FLAG_MASK = 1 << 4;

namespace GameBoyEmulator
{

template <std::endian compiling_device_endianness>
struct RegisterFile;

template <>
struct RegisterFile<std::endian::little>
{
    union
    {
        struct
        {
            uint8_t flags;
            uint8_t A;
        };
        uint16_t AF{};
    };
    union
    {
        struct
        {
            uint8_t C;
            uint8_t B;
        };
        uint16_t BC{};
    };
    union
    {
        struct
        {
            uint8_t E;
            uint8_t D;
        };
        uint16_t DE{};
    };
    union
    {
        struct
        {
            uint8_t L;
            uint8_t H;
        };
        uint16_t HL{};
    };
    uint16_t stack_pointer{};
    uint16_t program_counter{};
};

template <>
struct RegisterFile<std::endian::big>
{
    union
    {
        struct
        {
            uint8_t A;
            uint8_t flags;
        };
        uint16_t AF{};
    };
    union
    {
        struct
        {
            uint8_t B;
            uint8_t C;
        };
        uint16_t BC{};
    };
    union
    {
        struct
        {
            uint8_t D;
            uint8_t E;
        };
        uint16_t DE{};
    };
    union
    {
        struct
        {
            uint8_t H;
            uint8_t L;
        };
        uint16_t HL{};
    };
    uint16_t stack_pointer{};
    uint16_t program_counter{};
};

} // namespace GameBoyEmulator
