#pragma once

#include <bit>
#include <cstdint>

namespace GameBoy
{

constexpr uint8_t FLAG_ZERO_MASK = 1 << 7;
constexpr uint8_t FLAG_SUBTRACT_MASK = 1 << 6; // Also known as the 'N' flag
constexpr uint8_t FLAG_HALF_CARRY_MASK = 1 << 5; // For a carry from bit 3-4 or 11-12
constexpr uint8_t FLAG_CARRY_MASK = 1 << 4;

template <std::endian E>
struct RegisterFile;

template <>
struct RegisterFile<std::endian::little>
{
    union
	{
        struct
		{
            uint8_t flags;
            uint8_t a;
        };
        uint16_t af{};
    };
    union
	{
        struct
		{
            uint8_t c;
            uint8_t b;
        };
        uint16_t bc{};
    };
    union
	{
        struct
		{
            uint8_t e;
            uint8_t d;
        };
        uint16_t de{};
    };
    union
	{
        struct
		{
            uint8_t l;
            uint8_t h;
        };
        uint16_t hl{};
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
            uint8_t a;
            uint8_t flags;
        };
        uint16_t af{};
    };
    union
	{
        struct
		{
            uint8_t b;
            uint8_t c;
        };
        uint16_t bc{};
    };
    union
	{
        struct
		{
            uint8_t d;
            uint8_t e;
        };
        uint16_t de{};
    };
    union
	{
        struct
		{
            uint8_t h;
            uint8_t l;
        };
        uint16_t hl{};
    };
    uint16_t stack_pointer{};
    uint16_t program_counter{};
};

} // namespace GameBoy
