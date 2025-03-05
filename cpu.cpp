#include <iomanip>
#include <iostream>
#include "cpu.h"

namespace GameBoy {

void CPU::execute_instruction(std::uint8_t opcode) {
    if (opcode != 0xCB) {
        switch (opcode) {
            case 0x00: nop_0x00(); break;
            case 0x01: ld_bc_imm_16_0x01(); break;
            case 0x02: ld_mem_bc_a_0x02(); break;
            case 0x03: inc_bc_0x03(); break;
            case 0x04: inc_b_0x04(); break;
            case 0x05: dec_b_0x05(); break;
            case 0x06: ld_b_imm_8_0x06(); break;
            case 0x07: rlca_0x07(); break;
            case 0x08: ld_mem_imm_16_sp_0x08(); break;
            case 0x09: add_hl_bc_0x09(); break;
            case 0x0a: ld_a_mem_bc_0x0a(); break;
            case 0x0b: dec_bc_0x0b(); break;
            case 0x0c: inc_c_0x0c(); break;
            case 0x0d: dec_c_0x0d(); break;
            case 0x0e: ld_c_imm_8_0x0e(); break;
            case 0x0f: rrca_0x0f(); break;
            case 0x10: stop_imm_8_0x10(); break;
            case 0x11: ld_de_imm_16_0x11(); break;
            case 0x12: ld_mem_de_a_0x12(); break;
            case 0x13: inc_de_0x13(); break;
            case 0x14: inc_d_0x14(); break;
            case 0x15: dec_d_0x15(); break;
            case 0x16: ld_d_imm_8_0x16(); break;
            case 0x17: rla_0x17(); break;
            case 0x18: jr_sign_imm_8_0x18(); break;
            case 0x19: add_hl_de_0x19(); break;
            case 0x1a: ld_a_mem_de_0x1a(); break;
            case 0x1b: dec_de_0x1b(); break;
            case 0x1c: inc_e_0x1c(); break;
            case 0x1d: dec_e_0x1d(); break;
            case 0x1e: ld_e_imm_8_0x1e(); break;
            case 0x1f: rra_0x1f(); break;
            case 0x20: jr_nz_sign_imm_8_0x20(); break;
            case 0x21: ld_hl_imm_16_0x21(); break;
            case 0x22: ld_mem_hli_a_0x22(); break;
            case 0x23: inc_hl_0x23(); break;
            case 0x24: inc_h_0x24(); break;
            case 0x25: dec_h_0x25(); break;
            case 0x26: ld_h_imm_8_0x26(); break;
            case 0x27: daa_0x27(); break;
            case 0x28: jr_z_sign_imm_8_0x28(); break;
            case 0x29: add_hl_hl_0x29(); break;
            case 0x2a: ld_a_mem_hli_0x2a(); break;
            case 0x2b: dec_hl_0x2b(); break;
            case 0x2c: inc_l_0x2c(); break;
            case 0x2d: dec_l_0x2d(); break;
            case 0x2e: ld_l_imm_8_0x2e(); break;
            case 0x2f: cpl_0x2f(); break;
            default:
                std::cerr << std::hex << std::setfill('0');
                std::cerr << "Unimplemented instruction: 0x" << std::setw(2) << static_cast<int>(opcode) << "\n";
                break;
        }
    }
    else {
        std::uint8_t prefixed_opcode{memory.read_8(pc + 1)};
        switch (prefixed_opcode) {
            default:
                std::cerr << std::hex << std::setfill('0');
                std::cerr << "Unimplemented instruction: 0xCB" << std::setw(2) << static_cast<int>(prefixed_opcode) << "\n";
                break;
        }
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

void CPU::inc_reg_8(std::uint8_t &reg_8) {
    reg_8++;
    f = (reg_8 == 0) ? (f | FLAG_Z) : (f & ~FLAG_Z);
    f &= ~FLAG_N;
    f = ((reg_8 & 0x0F) == 0x00) ? (f | FLAG_H) : (f & ~FLAG_H);
    pc += 1;
    cycles_elapsed += 4;
}

void CPU::dec_reg_8(std::uint8_t &reg_8) {
    reg_8--;
    f = (reg_8 == 0) ? (f | FLAG_Z) : (f & ~FLAG_Z);
    f |= FLAG_N;
    f = ((reg_8 & 0x0F) == 0x0F) ? (f | FLAG_H) : (f & ~FLAG_H);
    pc += 1;
    cycles_elapsed += 4;
}

void CPU::inc_reg_16(std::uint16_t &reg_16) {
    reg_16++;
    pc += 1;
    cycles_elapsed += 8;
}

void CPU::dec_reg_16(std::uint16_t &reg_16) {
    reg_16--;
    pc += 1;
    cycles_elapsed += 8;
}

void CPU::ld_reg_8_imm_8(std::uint8_t &reg_8) {
    reg_8 = memory.read_8(pc + 1);
    pc += 2;
    cycles_elapsed += 8;
}

void CPU::ld_reg_8_mem_reg_16(std::uint8_t &reg_8, const std::uint16_t &reg_16) {
    reg_8 = memory.read_8(reg_16);
    pc += 1;
    cycles_elapsed += 8;
}

void CPU::ld_reg_16_imm_16(std::uint16_t &reg_16) {
    reg_16 = memory.read_16(pc + 1);
    pc += 3;
    cycles_elapsed += 12;
}

void CPU::ld_mem_reg_16_reg_8(std::uint16_t &reg_16, const std::uint8_t &reg_8) {
    memory.write_8(reg_16, reg_8);
    pc += 1;
    cycles_elapsed += 8;
}

void CPU::add_hl_reg_16(const std::uint16_t &reg_8) {
    std::uint16_t previous_hl{hl};
    hl += reg_8;
    f &= ~FLAG_N;
    f = (((previous_hl & 0x0FFF) + (reg_8 & 0x0FFF)) > 0x0FFF) ? (f | FLAG_H) : (f & ~FLAG_H);
    f = (previous_hl > hl) ? (f | FLAG_C) : (f & ~FLAG_C);
    pc += 1;
    cycles_elapsed += 8;
}

void CPU::jr_cond_sign_imm_8(bool condition) {
    if (condition) {
        int8_t offset{static_cast<int8_t>(memory.read_8(pc + 1))};
        pc += 2 + offset;
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

void CPU::ld_bc_imm_16_0x01() {
    ld_reg_16_imm_16(bc);
}

void CPU::ld_mem_bc_a_0x02() {
    ld_mem_reg_16_reg_8(bc, a);
}

void CPU::inc_bc_0x03() {
    inc_reg_16(bc);
}

void CPU::inc_b_0x04() {
    inc_reg_8(b);
}

void CPU::dec_b_0x05() {
    dec_reg_8(b);
}

void CPU::ld_b_imm_8_0x06() {
    ld_reg_8_imm_8(b);
}

void CPU::rlca_0x07() {
    a = (a << 1) | (a >> 7);
    f &= ~(FLAG_Z | FLAG_N | FLAG_H);
    f = ((a & 0b00000001) == 0b00000001) ? (f | FLAG_C) : (f & ~FLAG_C);
    pc += 1;
    cycles_elapsed += 4;
}

void CPU::ld_mem_imm_16_sp_0x08() {
    memory.write_16(memory.read_16(pc + 1), sp);
    pc += 3;
    cycles_elapsed += 20;
}

void CPU::add_hl_bc_0x09() {
    add_hl_reg_16(bc);
}

void CPU::ld_a_mem_bc_0x0a() {
    ld_reg_8_mem_reg_16(a, bc);
}

void CPU::dec_bc_0x0b() {
    dec_reg_16(bc);
}

void CPU::inc_c_0x0c() {
    inc_reg_8(c);
}

void CPU::dec_c_0x0d() {
    dec_reg_8(c);
}

void CPU::ld_c_imm_8_0x0e() {
    ld_reg_8_imm_8(c);
}

void CPU::rrca_0x0f() {
    a = (a >> 1) | (a << 7);
    f &= ~(FLAG_Z | FLAG_N | FLAG_H);
    f = ((a & 0b10000000) == 0b10000000) ? (f | FLAG_C) : (f & ~FLAG_C);
    pc += 1;
    cycles_elapsed += 4;
}

void CPU::stop_imm_8_0x10() {
    stopped = true;
    pc += 2; // Immediate isn't actually read - it's skipped over and usually will be 0x00
    cycles_elapsed += 4;
}

void CPU::ld_de_imm_16_0x11() {
    ld_reg_16_imm_16(de);
}

void CPU::ld_mem_de_a_0x12() {
    ld_mem_reg_16_reg_8(de, a);
}

void CPU::inc_de_0x13() {
    inc_reg_16(de);
}

void CPU::inc_d_0x14() {
    inc_reg_8(d);
}

void CPU::dec_d_0x15() {
    dec_reg_8(d);
}

void CPU::ld_d_imm_8_0x16() {
    ld_reg_8_imm_8(d);
}

void CPU::rla_0x17() {
    bool carry{(a & 0b10000000) == 0b10000000};
    a = (a << 1) | ((f & FLAG_C) >> 4);
    f &= ~(FLAG_Z | FLAG_N | FLAG_H);
    f = carry ? (f | FLAG_C) : (f & ~FLAG_C);
    pc += 1;
    cycles_elapsed += 4;
}

void CPU::jr_sign_imm_8_0x18() {
    jr_cond_sign_imm_8(true);
}

void CPU::add_hl_de_0x19() {
    add_hl_reg_16(de);
}

void CPU::ld_a_mem_de_0x1a() {
    ld_reg_8_mem_reg_16(a, de);
}

void CPU::dec_de_0x1b() {
    dec_reg_16(de);
}

void CPU::inc_e_0x1c() {
    inc_reg_8(e);
}

void CPU::dec_e_0x1d() {
    dec_reg_8(e);
}

void CPU::ld_e_imm_8_0x1e() {
    ld_reg_8_imm_8(e);
}

void CPU::rra_0x1f() {
    bool carry{(a & 0b00000001) == 0b00000001};
    a = (a >> 1) | ((f & FLAG_C) << 3);
    f &= ~(FLAG_Z | FLAG_N | FLAG_H);
    f = carry ? (f | FLAG_C) : (f & ~FLAG_C);
    pc += 1;
    cycles_elapsed += 4;
}

void CPU::jr_nz_sign_imm_8_0x20() {
    jr_cond_sign_imm_8((f & FLAG_Z) == 0);
}

void CPU::ld_hl_imm_16_0x21() {
    ld_reg_16_imm_16(hl);
}

void CPU::ld_mem_hli_a_0x22() {
    ld_mem_reg_16_reg_8(hl, a);
    hl++;
}

void CPU::inc_hl_0x23() {
    inc_reg_16(hl);
}

void CPU::inc_h_0x24() {
    inc_reg_8(h);
}

void CPU::dec_h_0x25() {
    dec_reg_8(h);
}

void CPU::ld_h_imm_8_0x26() {
    ld_reg_8_imm_8(h);
}

void CPU::daa_0x27() {
    // Previous operation was between two binary coded decimals (BCDs) and this corrects register a back to BCD format
    const bool previous_addition{(f & FLAG_N) == 0};
    bool carry{false};
    uint8_t correction{0};
    if ((f & FLAG_H) || (previous_addition && (a & 0x0F) > 0x09)) {
        correction |= 0x06;
    }
    if ((f & FLAG_C) || (previous_addition && a > 0x99)) {
        correction |= 0x60;
        carry = previous_addition;
    }
    a = previous_addition ? (a + correction) : (a - correction);
    f = (a == 0) ? (f | FLAG_Z) : (f & ~FLAG_Z);
    f &= ~FLAG_H;
    f = carry ? (f | FLAG_C) : (f & ~FLAG_C);
    pc += 1;
    cycles_elapsed += 4;
}

void CPU::jr_z_sign_imm_8_0x28() {
    jr_cond_sign_imm_8((f & FLAG_Z) != 0);
}

void CPU::add_hl_hl_0x29() {
    add_hl_reg_16(hl);
}

void CPU::ld_a_mem_hli_0x2a() {
    ld_reg_8_mem_reg_16(a, hl);
    hl++;
}

void CPU::dec_hl_0x2b() {
    dec_reg_16(hl);
}

void CPU::inc_l_0x2c() {
    inc_reg_8(l);
}

void CPU::dec_l_0x2d() {
    dec_reg_8(l);
}

void CPU::ld_l_imm_8_0x2e() {
    ld_reg_8_imm_8(l);
}

void CPU::cpl_0x2f() {
    a = ~a;
    f |= FLAG_N | FLAG_H;
    pc += 1;
    cycles_elapsed += 4;
}

} // namespace GameBoy
