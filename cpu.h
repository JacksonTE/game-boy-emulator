#pragma once
#include <cstdint>
#include "memory.h"

namespace GameBoy {

constexpr std::uint8_t FLAG_Z{1 << 7}; // Zero - set after arithmetic result is zero, cleared when result is nonzero
constexpr std::uint8_t FLAG_N{1 << 6}; // Subtract - set after subtraction/decrement/compare, cleared after addition/increment/logical operation (and LD HL, SP + e8)
constexpr std::uint8_t FLAG_H{1 << 5}; // Half-Carry (from bit 3-4, or sometimes from 11 to 12) - set when addition causes carry or subtraction requires borrow, cleared otherwise
constexpr std::uint8_t FLAG_C{1 << 4}; // Carry (from bit 7-8, or sometimes from 15-16) - set when addition causes carry, subtraction requires borrow, or bit shifted out, cleared otherwise

class CPU {
public:
    CPU(Memory mem) : memory{mem} {}
    void execute_instruction(std::uint8_t opcode);
    void print_values() const;

private:
    Memory memory;
    std::uint64_t cycles_elapsed{};
    bool stopped{};

    // 8-bit registers can be accessed individually or together through their corresponding 16-bit register pair
    // The first letter of each register pair is its most significant byte and the second is its least significant byte
    union {
        struct {
            std::uint8_t a;
            std::uint8_t f; // Flags register - only bits 7-4 are used (3-0 will always be zero)
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
    std::uint16_t sp{}; // Stack Pointer, address of the top of the stack in WRAM
    std::uint16_t pc{}; // Program Counter, address of the next instruction to execute

    // Shared logic for instruction functions
    void inc_reg_8(std::uint8_t &reg_8);
    void dec_reg_8(std::uint8_t &reg_8);
    void inc_reg_16(std::uint16_t &reg_16);
    void dec_reg_16(std::uint16_t &reg_16);
    void ld_reg_8_imm_8(std::uint8_t &reg_8);
    void ld_reg_8_mem_reg_16(std::uint8_t &reg_8, const std::uint16_t &reg_16);
    void ld_reg_16_imm_16(std::uint16_t &reg_16);
    void ld_mem_reg_16_reg_8(const std::uint16_t &reg_16, const std::uint8_t &reg_8);
    void add_hl_reg_16(const std::uint16_t &reg_16);
    void jr_cond_sign_imm_8(bool condition);

    // Instruction functions named after their mnemonic and suffixed with their opcode
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
};

} // namespace GameBoy
