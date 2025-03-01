#include <iomanip>
#include <iostream>
#include "cpu.h"

namespace GameBoy {
    void CPU::execute_instruction(std::uint8_t opcode) {
        if (opcode != 0xCB) {
            switch (opcode) {
                case 0x00: nop_0x00(); break;
                case 0x01: ld_bc_imm_0x01(); break;
                case 0x02: ld_mem_bc_a_0x02(); break;
                case 0x03: inc_bc_0x03(); break;
                case 0x04: inc_b_0x04(); break;
                case 0x05: dec_b_0x05(); break;
                case 0x06: ld_b_imm_0x06(); break;
                case 0x07: rlca_0x07(); break;
                case 0x08: ld_mem_imm_sp_0x08(); break;
                case 0x09: add_hl_bc_0x09(); break;
                case 0x0a: ld_a_mem_bc_0x0a(); break;
                case 0x0b: dec_bc_0x0b(); break;
                case 0x0c: inc_c_0x0c(); break;
                case 0x0d: dec_c_0x0d(); break;
                case 0x0e: ld_c_imm_0x0e(); break;
                case 0x0f: rrca_0x0f(); break;
                case 0x10: stop_imm_0x10(); break;
                case 0x11: ld_de_imm_0x11(); break;
                case 0x12: ld_mem_de_a_0x12(); break;
                case 0x13: inc_de_0x13(); break;
                case 0x14: inc_d_0x14(); break;
                case 0x15: dec_d_0x15(); break;
                case 0x16: ld_d_imm_0x16(); break;
                case 0x17: rla_0x17(); break;
                case 0x18: jr_sign_imm_0x18(); break;
                case 0x19: add_hl_de_0x19(); break;
                case 0x1a: ld_a_mem_de_0x1a(); break;
                case 0x1b: dec_de_0x1b(); break;
                case 0x1c: inc_e_0x1c(); break;
                case 0x1d: dec_e_0x1d(); break;
                case 0x1e: ld_e_imm_0x1e(); break;
                case 0x1f: rra_0x1f(); break;
                default:
                    std::cerr << std::hex << std::uppercase << std::setfill('0');
                    std::cerr << "Unimplemented instruction: 0x" << std::setw(2) << static_cast<int>(opcode) << std::endl;
                    break;
            }
        }
        else {
            std::uint8_t prefixed_opcode{memory.read_8(pc + 1)};
            switch (prefixed_opcode) {
                default:
                    std::cerr << std::hex << std::uppercase << std::setfill('0');
                    std::cerr << "Unimplemented instruction: 0xCB" << std::setw(2) << static_cast<int>(prefixed_opcode) << std::endl;
                    break;
            }
        }
    }

    void CPU::print_values() const {
        std::cout << "================== CPU Registers ==================" << std::endl;
        std::cout << std::hex << std::uppercase;
        std::cout << std::setfill('0');

        std::cout << "AF: 0x" << std::setw(4) << af
                  << "   (A: 0x" << std::setw(2) << static_cast<int>(a)
                  << ", F: 0x" << std::setw(2) << static_cast<int>(f) << ")" << std::endl;

        std::cout << "BC: 0x" << std::setw(4) << bc
                  << "   (B: 0x" << std::setw(2) << static_cast<int>(b)
                  << ", C: 0x" << std::setw(2) << static_cast<int>(c) << ")" << std::endl;

        std::cout << "DE: 0x" << std::setw(4) << de
                  << "   (D: 0x" << std::setw(2) << static_cast<int>(d)
                  << ", E: 0x" << std::setw(2) << static_cast<int>(e) << ")" << std::endl;

        std::cout << "HL: 0x" << std::setw(4) << hl
                  << "   (H: 0x" << std::setw(2) << static_cast<int>(h)
                  << ", L: 0x" << std::setw(2) << static_cast<int>(l) << ")" << std::endl;

        std::cout << "SP: 0x" << std::setw(4) << sp << std::endl;
        std::cout << "PC: 0x" << std::setw(4) << pc << std::endl;

        std::cout << std::dec;
        std::cout << "Cycles Elapsed: " << cycles_elapsed << std::endl;
        std::cout << "===================================================" << std::endl;
    }

    void CPU::inc_reg_8(uint8_t &reg) {
        reg++;
        f = (reg == 0) ? (f | FLAG_Z) : (f & ~FLAG_Z);
        f &= ~FLAG_N;
        f = ((reg & 0x0F) == 0x00) ? (f | FLAG_H) : (f & ~FLAG_H);
    }

    void CPU::dec_reg_8(uint8_t &reg) {
        reg--;
        f = (reg == 0) ? (f | FLAG_Z) : (f & ~FLAG_Z);
        f |= FLAG_N;
        f = ((reg & 0x0F) == 0x0F) ? (f | FLAG_H) : (f & ~FLAG_H);
    }

    void CPU::add_hl_reg_16(uint16_t &reg) {
        std::uint16_t previous_hl{hl};
        hl += reg;
        f &= ~FLAG_N;
        f = (((previous_hl & 0x0FFF) + (reg & 0x0FFF)) > 0x0FFF) ? (f | FLAG_H) : (f & ~FLAG_H);
        f = (previous_hl > hl) ? (f | FLAG_C) : (f & ~FLAG_C);
        pc += 1;
        cycles_elapsed += 8;
    }

    void CPU::nop_0x00() {
        pc += 1;
        cycles_elapsed += 4;
    }

    void CPU::ld_bc_imm_0x01() {
        bc = memory.read_16(pc + 1);
        pc += 3;
        cycles_elapsed += 12;
    }

    void CPU::ld_mem_bc_a_0x02() {
        memory.write_8(bc, a);
        pc += 1;
        cycles_elapsed += 8;
    }

    void CPU::inc_bc_0x03() {
        bc++;
        pc += 1;
        cycles_elapsed += 8;
    }

    void CPU::inc_b_0x04() {
        inc_reg_8(b);
        pc += 1;
        cycles_elapsed += 4;
    }

    void CPU::dec_b_0x05() {
        dec_reg_8(b);
        pc += 1;
        cycles_elapsed += 4;
    }

    void CPU::ld_b_imm_0x06() {
        b = memory.read_8(pc + 1);
        pc += 2;
        cycles_elapsed += 8;
    }

    void CPU::rlca_0x07() {
        a = (a << 1) | (a >> 7);
        f &= ~(FLAG_Z | FLAG_N | FLAG_H);
        f = ((a & 0b00000001) == 0b00000001) ? (f | FLAG_C) : (f & ~FLAG_C);
        pc += 1;
        cycles_elapsed += 4;
    }

    void CPU::ld_mem_imm_sp_0x08() {
        memory.write_16(memory.read_16(pc + 1), sp);
        pc += 3;
        cycles_elapsed += 20;
    }

    void CPU::add_hl_bc_0x09() {
        add_hl_reg_16(bc);
        pc += 1;
        cycles_elapsed += 8;
    }

    void CPU::ld_a_mem_bc_0x0a() {
        a = memory.read_8(bc);
        pc += 1;
        cycles_elapsed += 8;
    }

    void CPU::dec_bc_0x0b() {
        bc--;
        pc += 1;
        cycles_elapsed += 8;
    }

    void CPU::inc_c_0x0c() {
        inc_reg_8(c);
        pc += 1;
        cycles_elapsed += 4;
    }

    void CPU::dec_c_0x0d() {
        dec_reg_8(c);
        pc += 1;
        cycles_elapsed += 4;
    }

    void CPU::ld_c_imm_0x0e() {
        c = memory.read_8(pc + 1);
        pc += 2;
        cycles_elapsed += 8;
    }

    void CPU::rrca_0x0f() {
        a = (a >> 1) | (a << 7);
        f &= ~(FLAG_Z | FLAG_N | FLAG_H);
        f = ((a & 0b10000000) == 0b10000000) ? (f | FLAG_C) : (f & ~FLAG_C);
        pc += 1;
        cycles_elapsed += 4;
    }

    void CPU::stop_imm_0x10() {
        stopped = true;
        pc += 2; // Immediate isn't actually read and is skipped over, usually will be 0x00
        cycles_elapsed += 4;
    }

    void CPU::ld_de_imm_0x11() {
        de = memory.read_16(pc + 1);
        pc += 3;
        cycles_elapsed += 12;
    }

    void CPU::ld_mem_de_a_0x12() {
        memory.write_8(de, a);
        pc += 1;
        cycles_elapsed += 8;
    }

    void CPU::inc_de_0x13() {
        de++;
        pc += 1;
        cycles_elapsed += 8;
    }

    void CPU::inc_d_0x14() {
        inc_reg_8(d);
        pc += 1;
        cycles_elapsed += 4;
    }

    void CPU::dec_d_0x15() {
        dec_reg_8(d);
        pc += 1;
        cycles_elapsed += 4;
    }

    void CPU::ld_d_imm_0x16() {
        d = memory.read_8(pc + 1);
        pc += 2;
        cycles_elapsed += 8;
    }

    void CPU::rla_0x17() {
        bool carry{(a & 0b10000000) == 0b10000000};
        a = (a << 1) | ((f & FLAG_C) >> 4);
        f &= ~(FLAG_Z | FLAG_N | FLAG_H);
        f = carry ? (f | FLAG_C) : (f & ~FLAG_C);
        pc += 1;
        cycles_elapsed += 4;
    }

    void CPU::jr_sign_imm_0x18() {
        std::int8_t offset{static_cast<std::int8_t>(memory.read_8(pc + 1))};
        pc += 2 + offset;
        cycles_elapsed += 12;
    }

    void CPU::add_hl_de_0x19() {
        add_hl_reg_16(de);
        pc += 1;
        cycles_elapsed += 8;
    }

    void CPU::ld_a_mem_de_0x1a() {
        a = memory.read_8(de);
        pc += 1;
        cycles_elapsed += 8;
    }

    void CPU::dec_de_0x1b() {
        de--;
        pc += 1;
        cycles_elapsed += 8;
    }

    void CPU::inc_e_0x1c() {
        inc_reg_8(e);
        pc += 1;
        cycles_elapsed += 4;
    }

    void CPU::dec_e_0x1d() {
        dec_reg_8(e);
        pc += 1;
        cycles_elapsed += 4;
    }

    void CPU::ld_e_imm_0x1e() {
        e = memory.read_8(pc + 1);
        pc += 2;
        cycles_elapsed += 8;
    }

    void CPU::rra_0x1f() {
        bool carry{(a & 0b00000001) == 0b00000001};
        a = (a >> 1) | ((f & FLAG_C) << 3);
        f &= ~(FLAG_Z | FLAG_N | FLAG_H);
        f = carry ? (f | FLAG_C) : (f & ~FLAG_C);
        pc += 1;
        cycles_elapsed += 4;
    }
}
