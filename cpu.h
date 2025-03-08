#pragma once

#include <cstdint>
#include "memory.h"

namespace GameBoy {

constexpr uint8_t FLAG_Z{1 << 7}; // Zero - set after arithmetic result is zero, cleared when result is nonzero
constexpr uint8_t FLAG_N{1 << 6}; // Subtract - set after subtraction/decrement/compare, cleared after addition/increment/logical operation (and LD HL, SP + e8)
constexpr uint8_t FLAG_H{1 << 5}; // Half-Carry (from bit 3-4, or sometimes from 11 to 12) - set when addition causes carry or subtraction requires borrow, cleared otherwise
constexpr uint8_t FLAG_C{1 << 4}; // Carry (from bit 7-8, or sometimes from 15-16) - set when addition causes carry, subtraction requires borrow, or bit shifted out, cleared otherwise

class CPU {
public:
    CPU(Memory mem) : memory{mem} {}
    void print_values() const;
    void execute_instruction(uint8_t opcode);

private:
    Memory memory;
    bool stopped{};
    bool halted{};

    // 8-bit registers can be accessed individually or together through their corresponding 16-bit register pair
    // The first letter of each register pair is its most significant byte and the second is its least significant byte
    // *** Struct ordering currently assumes this runs on a little-endian system ***
    union {
        struct {
            uint8_t f; // Flags register - only bits 7-4 are used (3-0 will always be zero)
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
    uint16_t sp{}; // Stack Pointer, address of the top of the stack in WRAM
    uint16_t pc{}; // Program Counter, address of the next instruction byte to execute from memory
    uint64_t cycles_elapsed{};

    using Instruction = void (CPU::*)();
    static const Instruction instruction_table[256];
    static const Instruction cb_instruction_table[256];

    // Instruction Helpers
    void set_flags_z_n_h_c(bool set_z, bool set_n, bool set_h, bool set_c);
    void inc_reg8(uint8_t &reg);
    void dec_reg8(uint8_t &reg);
    void inc_reg16(uint16_t &reg);
    void dec_reg16(uint16_t &reg);
    void ld_reg8_reg8(uint8_t &dest, const uint8_t &src);
    void ld_reg8_imm8(uint8_t &dest);
    void ld_reg8_mem_reg16(uint8_t &dest, const uint16_t &src_addr);
    void ld_reg16_imm16(uint16_t &dest);
    void ld_mem_reg16_reg8(const uint16_t &dest_addr, const uint8_t &src);
    void add_a_reg8(const uint8_t &reg);
    void add_hl_reg16(const uint16_t &reg);
    void adc_a_reg8(const uint8_t &reg);
    void sub_a_reg8(const uint8_t &reg);
    void sbc_a_reg8(const uint8_t &reg);
    void and_a_reg8(const uint8_t &reg);
    void xor_a_reg8(const uint8_t &reg);
    void or_a_reg8(const uint8_t &reg);
    void cp_a_reg8(const uint8_t &reg);
    void jr_cond_sign_imm8(bool condition);

    // Instructions suffixed with their opcode
    void nop_0x00();
    void ld_bc_imm16_0x01();
    void ld_mem_bc_a_0x02();
    void inc_bc_0x03();
    void inc_b_0x04();
    void dec_b_0x05();
    void ld_b_imm8_0x06();
    void rlca_0x07();
    void ld_mem_imm16_sp_0x08();
    void add_hl_bc_0x09();
    void ld_a_mem_bc_0x0a();
    void dec_bc_0x0b();
    void inc_c_0x0c();
    void dec_c_0x0d();
    void ld_c_imm8_0x0e();
    void rrca_0x0f();
    void stop_imm8_0x10();
    void ld_de_imm16_0x11();
    void ld_mem_de_a_0x12();
    void inc_de_0x13();
    void inc_d_0x14();
    void dec_d_0x15();
    void ld_d_imm8_0x16();
    void rla_0x17();
    void jr_sign_imm8_0x18();
    void add_hl_de_0x19();
    void ld_a_mem_de_0x1a();
    void dec_de_0x1b();
    void inc_e_0x1c();
    void dec_e_0x1d();
    void ld_e_imm8_0x1e();
    void rra_0x1f();
    void jr_nz_sign_imm8_0x20();
    void ld_hl_imm16_0x21();
    void ld_mem_hli_a_0x22();
    void inc_hl_0x23();
    void inc_h_0x24();
    void dec_h_0x25();
    void ld_h_imm8_0x26();
    void daa_0x27();
    void jr_z_sign_imm8_0x28();
    void add_hl_hl_0x29();
    void ld_a_mem_hli_0x2a();
    void dec_hl_0x2b();
    void inc_l_0x2c();
    void dec_l_0x2d();
    void ld_l_imm8_0x2e();
    void cpl_0x2f();
    void jr_nc_sign_imm8_0x30();
    void ld_sp_imm16_0x31();
    void ld_mem_hld_a_0x32();
    void inc_sp_0x33();
    void inc_mem_hl_0x34();
    void dec_mem_hl_0x35();
    void ld_mem_hl_imm8_0x36();
    void scf_0x37();
    void jr_c_sign_imm8_0x38();
    void add_hl_sp_0x39();
    void ld_a_mem_hld_0x3a();
    void dec_sp_0x3b();
    void inc_a_0x3c();
    void dec_a_0x3d();
    void ld_a_imm8_0x3e();
    void ccf_0x3f();
    void ld_b_b_0x40();
    void ld_b_c_0x41();
    void ld_b_d_0x42();
    void ld_b_e_0x43();
    void ld_b_h_0x44();
    void ld_b_l_0x45();
    void ld_b_mem_hl_0x46();
    void ld_b_a_0x47();
    void ld_c_b_0x48();
    void ld_c_c_0x49();
    void ld_c_d_0x4a();
    void ld_c_e_0x4b();
    void ld_c_h_0x4c();
    void ld_c_l_0x4d();
    void ld_c_mem_hl_0x4e();
    void ld_c_a_0x4f();
    void ld_d_b_0x50();
    void ld_d_c_0x51();
    void ld_d_d_0x52();
    void ld_d_e_0x53();
    void ld_d_h_0x54();
    void ld_d_l_0x55();
    void ld_d_mem_hl_0x56();
    void ld_d_a_0x57();
    void ld_e_b_0x58();
    void ld_e_c_0x59();
    void ld_e_d_0x5a();
    void ld_e_e_0x5b();
    void ld_e_h_0x5c();
    void ld_e_l_0x5d();
    void ld_e_mem_hl_0x5e();
    void ld_e_a_0x5f();
    void ld_h_b_0x60();
    void ld_h_c_0x61();
    void ld_h_d_0x62();
    void ld_h_e_0x63();
    void ld_h_h_0x64();
    void ld_h_l_0x65();
    void ld_h_mem_hl_0x66();
    void ld_h_a_0x67();
    void ld_l_b_0x68();
    void ld_l_c_0x69();
    void ld_l_d_0x6a();
    void ld_l_e_0x6b();
    void ld_l_h_0x6c();
    void ld_l_l_0x6d();
    void ld_l_mem_hl_0x6e();
    void ld_l_a_0x6f();
    void ld_mem_hl_b_0x70();
    void ld_mem_hl_c_0x71();
    void ld_mem_hl_d_0x72();
    void ld_mem_hl_e_0x73();
    void ld_mem_hl_h_0x74();
    void ld_mem_hl_l_0x75();
    void halt_0x76();
    void ld_mem_hl_a_0x77();
    void ld_a_b_0x78();
    void ld_a_c_0x79();
    void ld_a_d_0x7a();
    void ld_a_e_0x7b();
    void ld_a_h_0x7c();
    void ld_a_l_0x7d();
    void ld_a_mem_hl_0x7e();
    void ld_a_a_0x7f();
    void add_a_b_0x80();
    void add_a_c_0x81();
    void add_a_d_0x82();
    void add_a_e_0x83();
    void add_a_h_0x84();
    void add_a_l_0x85();
    void add_a_mem_hl_0x86();
    void add_a_a_0x87();
    void adc_a_b_0x88();
    void adc_a_c_0x89();
    void adc_a_d_0x8a();
    void adc_a_e_0x8b();
    void adc_a_h_0x8c();
    void adc_a_l_0x8d();
    void adc_a_mem_hl_0x8e();
    void adc_a_a_0x8f();
    void sub_a_b_0x90();
    void sub_a_c_0x91();
    void sub_a_d_0x92();
    void sub_a_e_0x93();
    void sub_a_h_0x94();
    void sub_a_l_0x95();
    void sub_a_mem_hl_0x96();
    void sub_a_a_0x97();
    void sbc_a_b_0x98();
    void sbc_a_c_0x99();
    void sbc_a_d_0x9a();
    void sbc_a_e_0x9b();
    void sbc_a_h_0x9c();
    void sbc_a_l_0x9d();
    void sbc_a_mem_hl_0x9e();
    void sbc_a_a_0x9f();
    void and_a_b_0xa0();
    void and_a_c_0xa1();
    void and_a_d_0xa2();
    void and_a_e_0xa3();
    void and_a_h_0xa4();
    void and_a_l_0xa5();
    void and_a_mem_hl_0xa6();
    void and_a_a_0xa7();
    void xor_a_b_0xa8();
    void xor_a_c_0xa9();
    void xor_a_d_0xaa();
    void xor_a_e_0xab();
    void xor_a_h_0xac();
    void xor_a_l_0xad();
    void xor_a_mem_hl_0xae();
    void xor_a_a_0xaf();
    void or_a_b_0xb0();
    void or_a_c_0xb1();
    void or_a_d_0xb2();
    void or_a_e_0xb3();
    void or_a_h_0xb4();
    void or_a_l_0xb5();
    void or_a_mem_hl_0xb6();
    void or_a_a_0xb7();
    void cp_a_b_0xb8();
    void cp_a_c_0xb9();
    void cp_a_d_0xba();
    void cp_a_e_0xbb();
    void cp_a_h_0xbc();
    void cp_a_l_0xbd();
    void cp_a_mem_hl_0xbe();
    void cp_a_a_0xbf();
};

} // namespace GameBoy
