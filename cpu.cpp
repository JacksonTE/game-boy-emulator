#include <iomanip>
#include <iostream>
#include "cpu.h"

namespace GameBoy {

const CPU::Instruction CPU::instruction_table[256] = {
    &CPU::nop_0x00,
    &CPU::ld_bc_imm_16_0x01,
    &CPU::ld_mem_bc_a_0x02,
    &CPU::inc_bc_0x03,
    &CPU::inc_b_0x04,
    &CPU::dec_b_0x05,
    &CPU::ld_b_imm_8_0x06,
    &CPU::rlca_0x07,
    &CPU::ld_mem_imm_16_sp_0x08,
    &CPU::add_hl_bc_0x09,
    &CPU::ld_a_mem_bc_0x0a,
    &CPU::dec_bc_0x0b,
    &CPU::inc_c_0x0c,
    &CPU::dec_c_0x0d,
    &CPU::ld_c_imm_8_0x0e,
    &CPU::rrca_0x0f,
    &CPU::stop_imm_8_0x10,
    &CPU::ld_de_imm_16_0x11,
    &CPU::ld_mem_de_a_0x12,
    &CPU::inc_de_0x13,
    &CPU::inc_d_0x14,
    &CPU::dec_d_0x15,
    &CPU::ld_d_imm_8_0x16,
    &CPU::rla_0x17,
    &CPU::jr_sign_imm_8_0x18,
    &CPU::add_hl_de_0x19,
    &CPU::ld_a_mem_de_0x1a,
    &CPU::dec_de_0x1b,
    &CPU::inc_e_0x1c,
    &CPU::dec_e_0x1d,
    &CPU::ld_e_imm_8_0x1e,
    &CPU::rra_0x1f,
    &CPU::jr_nz_sign_imm_8_0x20,
    &CPU::ld_hl_imm_16_0x21,
    &CPU::ld_mem_hli_a_0x22,
    &CPU::inc_hl_0x23,
    &CPU::inc_h_0x24,
    &CPU::dec_h_0x25,
    &CPU::ld_h_imm_8_0x26,
    &CPU::daa_0x27,
    &CPU::jr_z_sign_imm_8_0x28,
    &CPU::add_hl_hl_0x29,
    &CPU::ld_a_mem_hli_0x2a,
    &CPU::dec_hl_0x2b,
    &CPU::inc_l_0x2c,
    &CPU::dec_l_0x2d,
    &CPU::ld_l_imm_8_0x2e,
    &CPU::cpl_0x2f,
    &CPU::jr_nc_sign_imm_8_0x30,
    &CPU::ld_sp_imm_16_0x31,
    &CPU::ld_mem_hld_a_0x32,
    &CPU::inc_sp_0x33,
    &CPU::inc_mem_hl_0x34,
    &CPU::dec_mem_hl_0x35,
    &CPU::ld_mem_hl_imm_8_0x36,
    &CPU::scf_0x37,
    &CPU::jr_c_sign_imm_8_0x38,
    &CPU::add_hl_sp_0x39,
    &CPU::ld_a_mem_hld_0x3a,
    &CPU::dec_sp_0x3b,
    &CPU::inc_a_0x3c,
    &CPU::dec_a_0x3d,
    &CPU::ld_a_imm_8_0x3e,
    &CPU::ccf_0x3f,
    &CPU::ld_b_b_0x40,
    &CPU::ld_b_c_0x41,
    &CPU::ld_b_d_0x42,
    &CPU::ld_b_e_0x43,
    &CPU::ld_b_h_0x44,
    &CPU::ld_b_l_0x45,
    &CPU::ld_b_mem_hl_0x46,
    &CPU::ld_b_a_0x47,
    &CPU::ld_c_b_0x48,
    &CPU::ld_c_c_0x49,
    &CPU::ld_c_d_0x4a,
    &CPU::ld_c_e_0x4b,
    &CPU::ld_c_h_0x4c,
    &CPU::ld_c_l_0x4d,
    &CPU::ld_c_mem_hl_0x4e,
    &CPU::ld_c_a_0x4f,
    &CPU::ld_d_b_0x50,
    &CPU::ld_d_c_0x51,
    &CPU::ld_d_d_0x52,
    &CPU::ld_d_e_0x53,
    &CPU::ld_d_h_0x54,
    &CPU::ld_d_l_0x55,
    &CPU::ld_d_mem_hl_0x56,
    &CPU::ld_d_a_0x57,
    &CPU::ld_e_b_0x58,
    &CPU::ld_e_c_0x59,
    &CPU::ld_e_d_0x5a,
    &CPU::ld_e_e_0x5b,
    &CPU::ld_e_h_0x5c,
    &CPU::ld_e_l_0x5d,
    &CPU::ld_e_mem_hl_0x5e,
    &CPU::ld_e_a_0x5f,
    &CPU::ld_h_b_0x60,
    &CPU::ld_h_c_0x61,
    &CPU::ld_h_d_0x62,
    &CPU::ld_h_e_0x63,
    &CPU::ld_h_h_0x64,
    &CPU::ld_h_l_0x65,
    &CPU::ld_h_mem_hl_0x66,
    &CPU::ld_h_a_0x67,
    &CPU::ld_l_b_0x68,
    &CPU::ld_l_c_0x69,
    &CPU::ld_l_d_0x6a,
    &CPU::ld_l_e_0x6b,
    &CPU::ld_l_h_0x6c,
    &CPU::ld_l_l_0x6d,
    &CPU::ld_l_mem_hl_0x6e,
    &CPU::ld_l_a_0x6f,
    &CPU::ld_mem_hl_b_0x70,
    &CPU::ld_mem_hl_c_0x71,
    &CPU::ld_mem_hl_d_0x72,
    &CPU::ld_mem_hl_e_0x73,
    &CPU::ld_mem_hl_h_0x74,
    &CPU::ld_mem_hl_l_0x75,
    &CPU::halt_0x76,
    &CPU::ld_mem_hl_a_0x77,
    &CPU::ld_a_b_0x78,
    &CPU::ld_a_c_0x79,
    &CPU::ld_a_d_0x7a,
    &CPU::ld_a_e_0x7b,
    &CPU::ld_a_h_0x7c,
    &CPU::ld_a_l_0x7d,
    &CPU::ld_a_mem_hl_0x7e,
    &CPU::ld_a_a_0x7f,
    &CPU::add_a_b_0x80,
    &CPU::add_a_c_0x81,
    &CPU::add_a_d_0x82,
    &CPU::add_a_e_0x83,
    &CPU::add_a_h_0x84,
    &CPU::add_a_l_0x85,
    &CPU::add_a_mem_hl_0x86,
    &CPU::add_a_a_0x87,
    &CPU::adc_a_b_0x88,
    &CPU::adc_a_c_0x89,
    &CPU::adc_a_d_0x8a,
    &CPU::adc_a_e_0x8b,
    &CPU::adc_a_h_0x8c,
    &CPU::adc_a_l_0x8d,
    &CPU::adc_a_mem_hl_0x8e,
    &CPU::adc_a_a_0x8f,
    &CPU::sub_a_b_0x90,
    &CPU::sub_a_c_0x91,
    &CPU::sub_a_d_0x92,
    &CPU::sub_a_e_0x93,
    &CPU::sub_a_h_0x94,
    &CPU::sub_a_l_0x95,
    &CPU::sub_a_mem_hl_0x96,
    &CPU::sub_a_a_0x97,
    &CPU::sbc_a_b_0x98,
    &CPU::sbc_a_c_0x99,
    &CPU::sbc_a_d_0x9a,
    &CPU::sbc_a_e_0x9b,
    &CPU::sbc_a_h_0x9c,
    &CPU::sbc_a_l_0x9d,
    &CPU::sbc_a_mem_hl_0x9e,
    &CPU::sbc_a_a_0x9f,
    &CPU::and_a_b_0xa0,
    &CPU::and_a_c_0xa1,
    &CPU::and_a_d_0xa2,
    &CPU::and_a_e_0xa3,
    &CPU::and_a_h_0xa4,
    &CPU::and_a_l_0xa5,
    &CPU::and_a_mem_hl_0xa6,
    &CPU::and_a_a_0xa7,
    &CPU::xor_a_b_0xa8,
    &CPU::xor_a_c_0xa9,
    &CPU::xor_a_d_0xaa,
    &CPU::xor_a_e_0xab,
    &CPU::xor_a_h_0xac,
    &CPU::xor_a_l_0xad,
    &CPU::xor_a_mem_hl_0xae,
    &CPU::xor_a_a_0xaf,
    &CPU::or_a_b_0xb0,
    &CPU::or_a_c_0xb1,
    &CPU::or_a_d_0xb2,
    &CPU::or_a_e_0xb3,
    &CPU::or_a_h_0xb4,
    &CPU::or_a_l_0xb5,
    &CPU::or_a_mem_hl_0xb6,
    &CPU::or_a_a_0xb7,
    &CPU::cp_a_b_0xb8,
    &CPU::cp_a_c_0xb9,
    &CPU::cp_a_d_0xba,
    &CPU::cp_a_e_0xbb,
    &CPU::cp_a_h_0xbc,
    &CPU::cp_a_l_0xbd,
    &CPU::cp_a_mem_hl_0xbe,
    &CPU::cp_a_a_0xbf,
};

const CPU::Instruction CPU::cb_instruction_table[256] = {
};

void CPU::execute_instruction(uint8_t opcode) {
    if (opcode != 0xCB) {
        (this->*instruction_table[opcode])();
    }
    else {
        uint8_t cb_opcode{memory.read_8(pc + 1)};
        (this->*cb_instruction_table[cb_opcode])();
    }
}

void CPU::print_values() const {
    std::cout << std::hex << std::setfill('0');
    std::cout << "================== CPU Registers ==================\n";

    std::cout << "AF: 0x" << std::setw(4) << af
            << "   (A: 0x" << std::setw(2) << static_cast<int>(a)
            << ", F: 0x" << std::setw(2) << static_cast<int>(f) << ")\n";

    std::cout << "BC: 0x" << std::setw(4) << bc
            << "   (B: 0x" << std::setw(2) << static_cast<int>(b)
            << ", C: 0x" << std::setw(2) << static_cast<int>(c) << ")\n";

    std::cout << "DE: 0x" << std::setw(4) << de
            << "   (D: 0x" << std::setw(2) << static_cast<int>(d)
            << ", E: 0x" << std::setw(2) << static_cast<int>(e) << ")\n";

    std::cout << "HL: 0x" << std::setw(4) << hl
            << "   (H: 0x" << std::setw(2) << static_cast<int>(h)
            << ", L: 0x" << std::setw(2) << static_cast<int>(l) << ")\n";

    std::cout << "SP: 0x" << std::setw(4) << sp << "\n";
    std::cout << "PC: 0x" << std::setw(4) << pc << "\n";

    std::cout << std::dec;
    std::cout << "Cycles Elapsed: " << cycles_elapsed << "\n";
    std::cout << "===================================================\n";
}

/******************************
 *    Instruction Helpers
 ******************************/

void CPU::set_flags_z_n_h_c(bool set_z, bool set_n, bool set_h, bool set_c) {
    f = (set_z ? FLAG_Z : 0) |
        (set_n ? FLAG_N : 0) |
        (set_h ? FLAG_H : 0) |
        (set_c ? FLAG_C : 0);
}

void CPU::inc_reg_8(uint8_t &reg) {
    const bool half_carry{(reg & 0x0F) == 0x0F};
    reg++;
    set_flags_z_n_h_c(reg == 0, false, half_carry, f & FLAG_C);
    pc += 1;
    cycles_elapsed += 4;
}

void CPU::dec_reg_8(uint8_t &reg) {
    const bool half_carry{(reg & 0x0F) == 0x00};
    reg--;
    set_flags_z_n_h_c(reg == 0, true, half_carry, f & FLAG_C);
    pc += 1;
    cycles_elapsed += 4;
}

void CPU::inc_reg_16(uint16_t &reg) {
    reg++;
    pc += 1;
    cycles_elapsed += 8;
}

void CPU::dec_reg_16(uint16_t &reg) {
    reg--;
    pc += 1;
    cycles_elapsed += 8;
}

void CPU::ld_reg_8_reg_8(uint8_t &dest, const uint8_t &src) {
    dest = src;
    pc += 1;
    cycles_elapsed += 4;
}

void CPU::ld_reg_8_imm_8(uint8_t &dest) {
    dest = memory.read_8(pc + 1);
    pc += 2;
    cycles_elapsed += 8;
}

void CPU::ld_reg_8_mem_reg_16(uint8_t &dest, const uint16_t &src_addr) {
    dest = memory.read_8(src_addr);
    pc += 1;
    cycles_elapsed += 8;
}

void CPU::ld_reg_16_imm_16(uint16_t &dest) {
    dest = memory.read_16(pc + 1);
    pc += 3;
    cycles_elapsed += 12;
}

void CPU::ld_mem_reg_16_reg_8(const uint16_t &dest_addr, const uint8_t &src) {
    memory.write_8(dest_addr, src);
    pc += 1;
    cycles_elapsed += 8;
}

void CPU::add_a_reg_8(const uint8_t &reg) {
    const bool half_carry{((a & 0x0F) + (reg & 0x0F)) > 0x0F};
    const bool carry{(static_cast<uint16_t>(a) + reg) > 0xFF};
    a += reg;
    set_flags_z_n_h_c(a == 0, false, half_carry, carry);
    pc += 1;
    cycles_elapsed += 4;
}

void CPU::add_hl_reg_16(const uint16_t &reg) {
    const bool half_carry{((hl & 0x0FFF) + (reg & 0x0FFF)) > 0x0FFF};
    const bool carry{(static_cast<uint32_t>(hl) + reg) > 0xFFFF};
    hl += reg;
    set_flags_z_n_h_c(f & FLAG_Z, false, half_carry, carry);
    pc += 1;
    cycles_elapsed += 8;
}

void CPU::adc_a_reg_8(const uint8_t &reg) {
    const uint8_t carry_in{static_cast<uint8_t>((f & FLAG_C) >> 4)};
    const bool half_carry{((a & 0x0F) + (reg & 0x0F) + carry_in) > 0x0F};
    const bool carry{(static_cast<uint16_t>(a) + reg + carry_in) > 0xFF};
    a += (reg + carry_in);
    set_flags_z_n_h_c(a == 0, false, half_carry, carry);
    pc += 1;
    cycles_elapsed += 4;
}

void CPU::sub_a_reg_8(const uint8_t &reg) {
    const bool half_carry{(a & 0x0F) < (reg & 0x0F)};
    const bool carry{a < reg};
    a -= reg;
    set_flags_z_n_h_c(a == 0, true, half_carry, carry);
    pc += 1;
    cycles_elapsed += 4;
}

void CPU::sbc_a_reg_8(const uint8_t &reg) {
    const uint8_t carry_in{static_cast<uint8_t>((f & FLAG_C) >> 4)};
    const bool half_carry{(a & 0x0F) < ((reg & 0x0F) + carry_in)};
    const bool carry{a < (reg + carry_in)};
    a -= (reg + carry_in);
    set_flags_z_n_h_c(a == 0, true, half_carry, carry);
    pc += 1;
    cycles_elapsed += 4;
}

void CPU::and_a_reg_8(const uint8_t &reg) {
    a &= reg;
    set_flags_z_n_h_c(a == 0, false, true, false);
    pc += 1;
    cycles_elapsed += 4;
}

void CPU::xor_a_reg_8(const uint8_t &reg) {
    a ^= reg;
    set_flags_z_n_h_c(a == 0, false, false, false);
    pc += 1;
    cycles_elapsed += 4;
}

void CPU::or_a_reg_8(const uint8_t &reg) {
    a |= reg;
    set_flags_z_n_h_c(a == 0, false, false, false);
    pc += 1;
    cycles_elapsed += 4;
}

void CPU::cp_a_reg_8(const uint8_t &reg) {
    const bool half_carry{(a & 0x0F) < (reg & 0x0F)};
    const bool carry{a < reg};
    set_flags_z_n_h_c(a == reg, true, half_carry, carry);
    pc += 1;
    cycles_elapsed += 4;
}

void CPU::jr_cond_sign_imm_8(bool condition) {
    if (condition) {
        pc += 2 + static_cast<int8_t>(memory.read_8(pc + 1));
        cycles_elapsed += 12;
    }
    else {
        pc += 2;
        cycles_elapsed += 8;
    }
}

/******************************
 *    Opcode Functions
 ******************************/

void CPU::nop_0x00() {
    pc += 1;
    cycles_elapsed += 4;
}

void CPU::ld_bc_imm_16_0x01() { ld_reg_16_imm_16(bc); }

void CPU::ld_mem_bc_a_0x02() { ld_mem_reg_16_reg_8(bc, a); }

void CPU::inc_bc_0x03() { inc_reg_16(bc); }

void CPU::inc_b_0x04() { inc_reg_8(b); }

void CPU::dec_b_0x05() { dec_reg_8(b); }

void CPU::ld_b_imm_8_0x06() { ld_reg_8_imm_8(b); }

void CPU::rlca_0x07() {
    const bool carry{(a & 0b10000000) == 0b10000000};
    a = (a << 1) | (a >> 7);
    set_flags_z_n_h_c(false, false, false, carry);
    pc += 1;
    cycles_elapsed += 4;
}

void CPU::ld_mem_imm_16_sp_0x08() {
    memory.write_16(memory.read_16(pc + 1), sp);
    pc += 3;
    cycles_elapsed += 20;
}

void CPU::add_hl_bc_0x09() { add_hl_reg_16(bc); }

void CPU::ld_a_mem_bc_0x0a() { ld_reg_8_mem_reg_16(a, bc); }

void CPU::dec_bc_0x0b() { dec_reg_16(bc); }

void CPU::inc_c_0x0c() { inc_reg_8(c); }

void CPU::dec_c_0x0d() { dec_reg_8(c); }

void CPU::ld_c_imm_8_0x0e() { ld_reg_8_imm_8(c); }

void CPU::rrca_0x0f() {
    const bool carry{(a & 0b00000001) == 0b00000001};
    a = (a >> 1) | (a << 7);
    set_flags_z_n_h_c(false, false, false, carry);
    pc += 1;
    cycles_elapsed += 4;
}

void CPU::stop_imm_8_0x10() {
    stopped = true;
    pc += 2; // Immediate isn't actually read - it's skipped over and usually will be 0x00
    cycles_elapsed += 4;
}

void CPU::ld_de_imm_16_0x11() { ld_reg_16_imm_16(de); }

void CPU::ld_mem_de_a_0x12() { ld_mem_reg_16_reg_8(de, a); }

void CPU::inc_de_0x13() { inc_reg_16(de); }

void CPU::inc_d_0x14() { inc_reg_8(d); }

void CPU::dec_d_0x15() { dec_reg_8(d); }

void CPU::ld_d_imm_8_0x16() { ld_reg_8_imm_8(d); }

void CPU::rla_0x17() {
    const bool carry{(a & 0b10000000) == 0b10000000};
    a = (a << 1) | ((f & FLAG_C) >> 4);
    set_flags_z_n_h_c(false, false, false, carry);
    pc += 1;
    cycles_elapsed += 4;
}

void CPU::jr_sign_imm_8_0x18() { jr_cond_sign_imm_8(true); }

void CPU::add_hl_de_0x19() { add_hl_reg_16(de); }

void CPU::ld_a_mem_de_0x1a() { ld_reg_8_mem_reg_16(a, de); }

void CPU::dec_de_0x1b() { dec_reg_16(de); }

void CPU::inc_e_0x1c() { inc_reg_8(e); }

void CPU::dec_e_0x1d() { dec_reg_8(e); }

void CPU::ld_e_imm_8_0x1e() { ld_reg_8_imm_8(e); }

void CPU::rra_0x1f() {
    const bool carry{(a & 0b00000001) == 0b00000001};
    a = (a >> 1) | ((f & FLAG_C) << 3);
    set_flags_z_n_h_c(false, false, false, carry);
    pc += 1;
    cycles_elapsed += 4;
}

void CPU::jr_nz_sign_imm_8_0x20() { jr_cond_sign_imm_8(!(f & FLAG_Z)); }

void CPU::ld_hl_imm_16_0x21() { ld_reg_16_imm_16(hl); }

void CPU::ld_mem_hli_a_0x22() {
    ld_mem_reg_16_reg_8(hl, a);
    hl++;
}

void CPU::inc_hl_0x23() { inc_reg_16(hl); }

void CPU::inc_h_0x24() { inc_reg_8(h); }

void CPU::dec_h_0x25() { dec_reg_8(h); }

void CPU::ld_h_imm_8_0x26() { ld_reg_8_imm_8(h); }

void CPU::daa_0x27() {
    // Previous operation was between two binary coded decimals (BCDs) and this corrects register a back to BCD format
    const bool previous_addition{!(f & FLAG_N)};
    bool carry{false};
    uint8_t correction{0};
    if ((f & FLAG_H) || (previous_addition && ((a & 0x0F) > 0x09))) {
        correction |= 0x06;
    }
    if ((f & FLAG_C) || (previous_addition && (a > 0x99))) {
        correction |= 0x60;
        carry = previous_addition;
    }
    a = previous_addition ? (a + correction) : (a - correction);
    set_flags_z_n_h_c(a == 0, f & FLAG_N, false, carry);
    pc += 1;
    cycles_elapsed += 4;
}

void CPU::jr_z_sign_imm_8_0x28() { jr_cond_sign_imm_8(f & FLAG_Z); }

void CPU::add_hl_hl_0x29() { add_hl_reg_16(hl); }

void CPU::ld_a_mem_hli_0x2a() {
    ld_reg_8_mem_reg_16(a, hl);
    hl++;
}

void CPU::dec_hl_0x2b() { dec_reg_16(hl); }

void CPU::inc_l_0x2c() { inc_reg_8(l); }

void CPU::dec_l_0x2d() { dec_reg_8(l); }

void CPU::ld_l_imm_8_0x2e() { ld_reg_8_imm_8(l); }

void CPU::cpl_0x2f() {
    a = ~a;
    set_flags_z_n_h_c(f & FLAG_Z, true, true, f & FLAG_H);
    pc += 1;
    cycles_elapsed += 4;
}

void CPU::jr_nc_sign_imm_8_0x30() { jr_cond_sign_imm_8(!(f & FLAG_C)); }

void CPU::ld_sp_imm_16_0x31() { ld_reg_16_imm_16(sp); }

void CPU::ld_mem_hld_a_0x32() {
    ld_mem_reg_16_reg_8(hl, a);
    hl--;
}

void CPU::inc_sp_0x33() { inc_reg_16(sp); }

void CPU::inc_mem_hl_0x34() {
    uint8_t mem_hl{memory.read_8(hl)};
    const bool half_carry{(mem_hl & 0x0F) == 0x0F};
    mem_hl++;
    memory.write_8(hl, mem_hl);
    set_flags_z_n_h_c(mem_hl == 0, false, half_carry, f & FLAG_C);
    pc += 1;
    cycles_elapsed += 12;
}

void CPU::dec_mem_hl_0x35() {
    uint8_t mem_hl{memory.read_8(hl)};
    const bool half_carry{(mem_hl & 0x0F) == 0x00};
    mem_hl--;
    memory.write_8(hl, mem_hl);
    set_flags_z_n_h_c(mem_hl == 0, true, half_carry, f & FLAG_C);
    pc += 1;
    cycles_elapsed += 12;
}

void CPU::ld_mem_hl_imm_8_0x36() {
    memory.write_8(hl, memory.read_8(pc + 1));
    pc += 2;
    cycles_elapsed += 12;
}

void CPU::scf_0x37() {
    set_flags_z_n_h_c(f & FLAG_Z, false, false, true);
    pc += 1;
    cycles_elapsed += 4;
}

void CPU::jr_c_sign_imm_8_0x38() { jr_cond_sign_imm_8(f & FLAG_C); }

void CPU::add_hl_sp_0x39() { add_hl_reg_16(sp); }

void CPU::ld_a_mem_hld_0x3a() {
    ld_reg_8_mem_reg_16(a, hl);
    hl--;
}

void CPU::dec_sp_0x3b() { dec_reg_16(sp); }

void CPU::inc_a_0x3c() { inc_reg_8(a); }

void CPU::dec_a_0x3d() { dec_reg_8(a); }

void CPU::ld_a_imm_8_0x3e() { ld_reg_8_imm_8(a); }

void CPU::ccf_0x3f() {
    set_flags_z_n_h_c(f & FLAG_Z, false, false, !(f & FLAG_C));
    pc += 1;
    cycles_elapsed += 4;
}

void CPU::ld_b_b_0x40() { ld_reg_8_reg_8(b, b); } // No effect, but still advances pc/cycles_elapsed

void CPU::ld_b_c_0x41() { ld_reg_8_reg_8(b, c); }

void CPU::ld_b_d_0x42() { ld_reg_8_reg_8(b, d); }

void CPU::ld_b_e_0x43() { ld_reg_8_reg_8(b, e); }

void CPU::ld_b_h_0x44() { ld_reg_8_reg_8(b, h); }

void CPU::ld_b_l_0x45() { ld_reg_8_reg_8(b, l); }

void CPU::ld_b_mem_hl_0x46() { ld_reg_8_mem_reg_16(b, hl); }

void CPU::ld_b_a_0x47() { ld_reg_8_reg_8(b, a); }

void CPU::ld_c_b_0x48() { ld_reg_8_reg_8(c, b); }

void CPU::ld_c_c_0x49() { ld_reg_8_reg_8(c, c); } // No effect, but still advances pc/cycles_elapsed

void CPU::ld_c_d_0x4a() { ld_reg_8_reg_8(c, d); }

void CPU::ld_c_e_0x4b() { ld_reg_8_reg_8(c, e); }

void CPU::ld_c_h_0x4c() { ld_reg_8_reg_8(c, h); }

void CPU::ld_c_l_0x4d() { ld_reg_8_reg_8(c, l); }

void CPU::ld_c_mem_hl_0x4e() { ld_reg_8_mem_reg_16(c, hl); }

void CPU::ld_c_a_0x4f() { ld_reg_8_reg_8(c, a); }

void CPU::ld_d_b_0x50() { ld_reg_8_reg_8(d, b); }

void CPU::ld_d_c_0x51() { ld_reg_8_reg_8(d, c); }

void CPU::ld_d_d_0x52() { ld_reg_8_reg_8(d, d); } // No effect, but still advances pc/cycles_elapsed

void CPU::ld_d_e_0x53() { ld_reg_8_reg_8(d, e); }

void CPU::ld_d_h_0x54() { ld_reg_8_reg_8(d, h); }

void CPU::ld_d_l_0x55() { ld_reg_8_reg_8(d, l); }

void CPU::ld_d_mem_hl_0x56() { ld_reg_8_mem_reg_16(d, hl); }

void CPU::ld_d_a_0x57() { ld_reg_8_reg_8(d, a); }

void CPU::ld_e_b_0x58() { ld_reg_8_reg_8(e, b); }

void CPU::ld_e_c_0x59() { ld_reg_8_reg_8(e, c); }

void CPU::ld_e_d_0x5a() { ld_reg_8_reg_8(e, d); }

void CPU::ld_e_e_0x5b() { ld_reg_8_reg_8(e, e); } // No effect, but still advances pc/cycles_elapsed

void CPU::ld_e_h_0x5c() { ld_reg_8_reg_8(e, h); }

void CPU::ld_e_l_0x5d() { ld_reg_8_reg_8(e, l); }

void CPU::ld_e_mem_hl_0x5e() { ld_reg_8_mem_reg_16(e, hl); }

void CPU::ld_e_a_0x5f() { ld_reg_8_reg_8(e, a); }

void CPU::ld_h_b_0x60() { ld_reg_8_reg_8(h, b); }

void CPU::ld_h_c_0x61() { ld_reg_8_reg_8(h, c); }

void CPU::ld_h_d_0x62() { ld_reg_8_reg_8(h, d); }

void CPU::ld_h_e_0x63() { ld_reg_8_reg_8(h, e); }

void CPU::ld_h_h_0x64() { ld_reg_8_reg_8(h, h); }  // No effect, but still advances pc/cycles_elapsed

void CPU::ld_h_l_0x65() { ld_reg_8_reg_8(h, l); }

void CPU::ld_h_mem_hl_0x66() { ld_reg_8_mem_reg_16(h, hl); }

void CPU::ld_h_a_0x67() { ld_reg_8_reg_8(h, a); }

void CPU::ld_l_b_0x68() { ld_reg_8_reg_8(l, b); }

void CPU::ld_l_c_0x69() { ld_reg_8_reg_8(l, c); }

void CPU::ld_l_d_0x6a() { ld_reg_8_reg_8(l, d); }

void CPU::ld_l_e_0x6b() { ld_reg_8_reg_8(l, e); }

void CPU::ld_l_h_0x6c() { ld_reg_8_reg_8(l, h); }

void CPU::ld_l_l_0x6d() { ld_reg_8_reg_8(l, l); }  // No effect, but still advances pc/cycles_elapsed

void CPU::ld_l_mem_hl_0x6e() { ld_reg_8_mem_reg_16(l, hl); }

void CPU::ld_l_a_0x6f() { ld_reg_8_reg_8(l, a); }

void CPU::ld_mem_hl_b_0x70() { ld_mem_reg_16_reg_8(hl, b); }

void CPU::ld_mem_hl_c_0x71() { ld_mem_reg_16_reg_8(hl, c); }

void CPU::ld_mem_hl_d_0x72() { ld_mem_reg_16_reg_8(hl, d); }

void CPU::ld_mem_hl_e_0x73() { ld_mem_reg_16_reg_8(hl, e); }

void CPU::ld_mem_hl_h_0x74() { ld_mem_reg_16_reg_8(hl, h); }

void CPU::ld_mem_hl_l_0x75() { ld_mem_reg_16_reg_8(hl, l); }

void CPU::halt_0x76() {
    halted = true;
    pc += 1;
    cycles_elapsed += 4;
}

void CPU::ld_mem_hl_a_0x77() { ld_mem_reg_16_reg_8(hl, a); }

void CPU::ld_a_b_0x78() { ld_reg_8_reg_8(a, b); }

void CPU::ld_a_c_0x79() { ld_reg_8_reg_8(a, c); }

void CPU::ld_a_d_0x7a() { ld_reg_8_reg_8(a, d); }

void CPU::ld_a_e_0x7b() { ld_reg_8_reg_8(a, e); }

void CPU::ld_a_h_0x7c() { ld_reg_8_reg_8(a, h); }

void CPU::ld_a_l_0x7d() { ld_reg_8_reg_8(a, l); }

void CPU::ld_a_mem_hl_0x7e() { ld_reg_8_mem_reg_16(a, hl); }

void CPU::ld_a_a_0x7f() { ld_reg_8_reg_8(a, a); } // No effect, but still advances pc/cycles_elapsed

void CPU::add_a_b_0x80() { add_a_reg_8(b); }

void CPU::add_a_c_0x81() { add_a_reg_8(c); }

void CPU::add_a_d_0x82() { add_a_reg_8(d); }

void CPU::add_a_e_0x83() { add_a_reg_8(e); }

void CPU::add_a_h_0x84() { add_a_reg_8(h); }

void CPU::add_a_l_0x85() { add_a_reg_8(l); }

void CPU::add_a_mem_hl_0x86() {
    add_a_reg_8(memory.read_8(hl));
    cycles_elapsed += 4; // Takes 4 cycles more for memory read than register access
}

void CPU::add_a_a_0x87() { add_a_reg_8(a); }

void CPU::adc_a_b_0x88() { adc_a_reg_8(b); }

void CPU::adc_a_c_0x89() { adc_a_reg_8(c); }

void CPU::adc_a_d_0x8a() { adc_a_reg_8(d); }

void CPU::adc_a_e_0x8b() { adc_a_reg_8(e); }

void CPU::adc_a_h_0x8c() { adc_a_reg_8(h); }

void CPU::adc_a_l_0x8d() { adc_a_reg_8(l); }

void CPU::adc_a_mem_hl_0x8e() {
    adc_a_reg_8(memory.read_8(hl));
    cycles_elapsed += 4; // Takes 4 cycles more for memory read than register access
}

void CPU::adc_a_a_0x8f() { adc_a_reg_8(a); }

void CPU::sub_a_b_0x90() { sub_a_reg_8(b); }

void CPU::sub_a_c_0x91() { sub_a_reg_8(c); }

void CPU::sub_a_d_0x92() { sub_a_reg_8(d); }

void CPU::sub_a_e_0x93() { sub_a_reg_8(e); }

void CPU::sub_a_h_0x94() { sub_a_reg_8(h); }

void CPU::sub_a_l_0x95() { sub_a_reg_8(l); }

void CPU::sub_a_mem_hl_0x96() {
    sub_a_reg_8(memory.read_8(hl));
    cycles_elapsed += 4; // Takes 4 cycles more for memory read than register access
}

void CPU::sub_a_a_0x97() { sub_a_reg_8(a); }

void CPU::sbc_a_b_0x98() { sbc_a_reg_8(b); }

void CPU::sbc_a_c_0x99() { sbc_a_reg_8(c); }

void CPU::sbc_a_d_0x9a() { sbc_a_reg_8(d); }

void CPU::sbc_a_e_0x9b() { sbc_a_reg_8(e); }

void CPU::sbc_a_h_0x9c() { sbc_a_reg_8(h); }

void CPU::sbc_a_l_0x9d() { sbc_a_reg_8(l); }

void CPU::sbc_a_mem_hl_0x9e() {
    sbc_a_reg_8(memory.read_8(hl));
    cycles_elapsed += 4; // Takes 4 cycles more for memory read than register access
}

void CPU::sbc_a_a_0x9f() { sbc_a_reg_8(a); }

void CPU::and_a_b_0xa0() { and_a_reg_8(b); }

void CPU::and_a_c_0xa1() { and_a_reg_8(c); }

void CPU::and_a_d_0xa2() { and_a_reg_8(d); }

void CPU::and_a_e_0xa3() { and_a_reg_8(e); }

void CPU::and_a_h_0xa4() { and_a_reg_8(h); }

void CPU::and_a_l_0xa5() { and_a_reg_8(l); }

void CPU::and_a_mem_hl_0xa6() {
    and_a_reg_8(memory.read_8(hl));
    cycles_elapsed += 4; // Takes 4 cycles more for memory read than register access
}

void CPU::and_a_a_0xa7() { and_a_reg_8(a); }

void CPU::xor_a_b_0xa8() { xor_a_reg_8(b); }

void CPU::xor_a_c_0xa9() { xor_a_reg_8(c); }

void CPU::xor_a_d_0xaa() { xor_a_reg_8(d); }

void CPU::xor_a_e_0xab() { xor_a_reg_8(e); }

void CPU::xor_a_h_0xac() { xor_a_reg_8(h); }

void CPU::xor_a_l_0xad() { xor_a_reg_8(l); }

void CPU::xor_a_mem_hl_0xae() {
    xor_a_reg_8(memory.read_8(hl));
    cycles_elapsed += 4; // Takes 4 cycles more for memory read than register access
}

void CPU::xor_a_a_0xaf() { xor_a_reg_8(a); }

void CPU::or_a_b_0xb0() { or_a_reg_8(b); }

void CPU::or_a_c_0xb1() { or_a_reg_8(c); }

void CPU::or_a_d_0xb2() { or_a_reg_8(d); }

void CPU::or_a_e_0xb3() { or_a_reg_8(e); }

void CPU::or_a_h_0xb4() { or_a_reg_8(h); }

void CPU::or_a_l_0xb5() { or_a_reg_8(l); }

void CPU::or_a_mem_hl_0xb6() {
    or_a_reg_8(memory.read_8(hl));
    cycles_elapsed += 4; // Takes 4 cycles more for memory read than register access
}

void CPU::or_a_a_0xb7() { or_a_reg_8(a); }

void CPU::cp_a_b_0xb8() { cp_a_reg_8(b); }

void CPU::cp_a_c_0xb9() { cp_a_reg_8(c); }

void CPU::cp_a_d_0xba() { cp_a_reg_8(d); }

void CPU::cp_a_e_0xbb() { cp_a_reg_8(e); }

void CPU::cp_a_h_0xbc() { cp_a_reg_8(h); }

void CPU::cp_a_l_0xbd() { cp_a_reg_8(l); }

void CPU::cp_a_mem_hl_0xbe() {
    cp_a_reg_8(memory.read_8(hl));
    cycles_elapsed += 4; // Takes 4 cycles more for memory read than register access
}

void CPU::cp_a_a_0xbf() { cp_a_reg_8(a); }

} // namespace GameBoy
