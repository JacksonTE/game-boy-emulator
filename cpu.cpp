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
                default:
                    std::cerr << std::hex << std::uppercase << std::setfill('0');
                    std::cerr << "Unimplemented instruction: 0x" << std::setw(2) << static_cast<int>(opcode) << std::endl;
                    break;
            }
        }
        else {
            std::uint8_t prefixed_opcode = memory.read_8(pc + 1);
            switch (prefixed_opcode) {
                default:
                    std::cerr << std::hex << std::uppercase << std::setfill('0');
                    std::cerr << "Unimplemented instruction: 0xCB" << std::setw(2) << static_cast<int>(prefixed_opcode) << std::endl;
                    break;
            }
        }
    }

    void CPU::print_internal_values() const {
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
        bool half_carry = ((reg & 0x0F) == 0x0F);
        reg++;
        f = (reg == 0) ? (f | FLAG_Z) : (f & ~FLAG_Z);
        f &= ~FLAG_N;
        f = half_carry ? (f | FLAG_H) : (f & ~FLAG_H);
    }

    void CPU::dec_reg_8(uint8_t &reg) {
        bool half_carry = ((reg & 0x0F) == 0x00);
        reg--;
        f = (reg == 0) ? (f | FLAG_Z) : (f & ~FLAG_Z);
        f |= FLAG_N;
        f = half_carry ? (f | FLAG_H) : (f & ~FLAG_H);
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
        bool carry = ((a & 0b10000000) == 0b10000000);
        a = (a << 1) | (a >> 7);
        f &= ~(FLAG_Z | FLAG_N | FLAG_H);
        f = carry ? (f | FLAG_C) : (f & ~FLAG_C);
        pc += 1;
        cycles_elapsed += 4;
    }

    void CPU::ld_mem_imm_sp_0x08() {
        memory.write_16(memory.read_16(pc + 1), sp);
        pc += 3;
        cycles_elapsed += 20;
    }

    void CPU::add_hl_bc_0x09() {
        std::uint32_t result = hl + bc;
        bool half_carry = (((hl & 0x0FFF) + (bc & 0x0FFF)) > 0x0FFF);
        bool carry = result > 0xFFFF;
        hl = static_cast<uint16_t>(result);
        f &= ~FLAG_N;
        f = half_carry ? (f | FLAG_H) : (f & ~FLAG_H);
        f = carry ? (f | FLAG_C) : (f & ~FLAG_C);
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
        bool carry = ((a & 0b00000001) == 0b00000001);
        a = (a >> 1) | (a << 7);
        f &= ~(FLAG_Z | FLAG_N | FLAG_H);
        f = carry ? (f | FLAG_C) : (f & ~FLAG_C);
        pc += 1;
        cycles_elapsed += 4;
    }
}
