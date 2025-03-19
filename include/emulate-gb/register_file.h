#pragma once

#include <bit>
#include <cstdint>

namespace GameBoy {

template <std::endian E>
struct RegisterFile;

template <>
struct RegisterFile<std::endian::little> {
    union {
        struct {
            uint8_t flags;
            uint8_t a;
        };
        uint16_t af{};
    };
    union {
        struct {
            uint8_t c;
            uint8_t b;
        };
        uint16_t bc{};
    };
    union {
        struct {
            uint8_t e;
            uint8_t d;
        };
        uint16_t de{};
    };
    union {
        struct {
            uint8_t l;
            uint8_t h;
        };
        uint16_t hl{};
    };
    uint16_t stack_pointer{};
    uint16_t program_counter{};
};

template <>
struct RegisterFile<std::endian::big> {
    union {
        struct {
            uint8_t a;
            uint8_t flags;
        };
        uint16_t af{};
    };
    union {
        struct {
            uint8_t b;
            uint8_t c;
        };
        uint16_t bc{};
    };
    union {
        struct {
            uint8_t d;
            uint8_t e;
        };
        uint16_t de{};
    };
    union {
        struct {
            uint8_t h;
            uint8_t l;
        };
        uint16_t hl{};
    };
    uint16_t stack_pointer{};
    uint16_t program_counter{};
};

} // namespace GameBoy
