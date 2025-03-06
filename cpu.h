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
    void execute_instruction(uint8_t opcode);
    void print_values() const;

private:
    Memory memory;
    uint64_t cycles_elapsed{};
    bool stopped{};
    bool halted{};

    // 8-bit registers can be accessed individually or together through their corresponding 16-bit register pair
    // The first letter of each register pair is its most significant byte and the second is its least significant byte
    union {
        struct {
            uint8_t a;
            uint8_t f; // Flags register - only bits 7-4 are used (3-0 will always be zero)
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
    uint16_t sp{}; // Stack Pointer, address of the top of the stack in WRAM
    uint16_t pc{}; // Program Counter, address of the next instruction to execute

    // Instruction Helpers
    void set_flags_z_n_h_c(bool cond_z, bool cond_n, bool cond_h, bool cond_c);
    void inc_reg_8(uint8_t &reg);
    void dec_reg_8(uint8_t &reg);
    void inc_reg_16(uint16_t &reg);
    void dec_reg_16(uint16_t &reg);
    void ld_reg_8_reg_8(uint8_t &dest, const uint8_t &src);
    void ld_reg_8_imm_8(uint8_t &dest);
    void ld_reg_8_mem_reg_16(uint8_t &dest, const uint16_t &src_addr);
    void ld_reg_16_imm_16(uint16_t &dest);
    void ld_mem_reg_16_reg_8(const uint16_t &dest_addr, const uint8_t &src);
    void add_reg_8_reg_8(uint8_t &op_1, const uint8_t &op_2);
    void add_hl_reg_16(const uint16_t &reg);
    void adc_reg_8_reg_8(uint8_t &op_1, const uint8_t &op_2);
    void sub_a_reg_8(const uint8_t &reg);
    void jr_cond_sign_imm_8(bool condition);

    // Instruction functions suffixed with their opcode
    // imm_n - Next n bits in memory (i.e. memory[pc + 1])
    // sign_imm_8 - Next byte in memory treated as a signed offset stored in 2s complement
    // mem_ - The following register/immediate refers to a location in memory (e.g. mem_bc means memory[bc])
    // regi/regd - Increment/decrement register reg after operation (e.g. ld_mem_hli_a means increment hl after load)
    void nop_0x00();
    void ld_bc_imm_16_0x01();
    void ld_mem_bc_a_0x02();
    void inc_bc_0x03();
    void inc_b_0x04();
    void dec_b_0x05();
    void ld_b_imm_8_0x06();
    void rlca_0x07();
    void ld_mem_imm_16_sp_0x08();
    void add_hl_bc_0x09();
    void ld_a_mem_bc_0x0a();
    void dec_bc_0x0b();
    void inc_c_0x0c();
    void dec_c_0x0d();
    void ld_c_imm_8_0x0e();
    void rrca_0x0f();
    void stop_imm_8_0x10();
    void ld_de_imm_16_0x11();
    void ld_mem_de_a_0x12();
    void inc_de_0x13();
    void inc_d_0x14();
    void dec_d_0x15();
    void ld_d_imm_8_0x16();
    void rla_0x17();
    void jr_sign_imm_8_0x18();
    void add_hl_de_0x19();
    void ld_a_mem_de_0x1a();
    void dec_de_0x1b();
    void inc_e_0x1c();
    void dec_e_0x1d();
    void ld_e_imm_8_0x1e();
    void rra_0x1f();
    void jr_nz_sign_imm_8_0x20();
    void ld_hl_imm_16_0x21();
    void ld_mem_hli_a_0x22();
    void inc_hl_0x23();
    void inc_h_0x24();
    void dec_h_0x25();
    void ld_h_imm_8_0x26();
    void daa_0x27();
    void jr_z_sign_imm_8_0x28();
    void add_hl_hl_0x29();
    void ld_a_mem_hli_0x2a();
    void dec_hl_0x2b();
    void inc_l_0x2c();
    void dec_l_0x2d();
    void ld_l_imm_8_0x2e();
    void cpl_0x2f();
    void jr_nc_sign_imm_8_0x30();
    void ld_sp_imm_16_0x31();
    void ld_mem_hld_a_0x32();
    void inc_sp_0x33();
    void inc_mem_hl_0x34();
    void dec_mem_hl_0x35();
    void ld_mem_hl_imm_8_0x36();
    void scf_0x37();
    void jr_c_sign_imm_8_0x38();
    void add_hl_sp_0x39();
    void ld_a_mem_hld_0x3a();
    void dec_sp_0x3b();
    void inc_a_0x3c();
    void dec_a_0x3d();
    void ld_a_imm_8_0x3e();
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
};

} // namespace GameBoy
