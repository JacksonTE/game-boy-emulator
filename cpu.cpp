#include <iomanip>
#include <iostream>
#include "cpu.h"

namespace GameBoy {

void CPU::print_register_values() const {
    std::cout << std::hex << std::setfill('0');
    std::cout << "================== CPU Registers ==================\n";

    std::cout << "AF: 0x" << std::setw(4) << registers.af << "   "
              << "(A: 0x" << std::setw(2) << static_cast<int>(registers.a)
              << ", F: 0x" << std::setw(2) << static_cast<int>(registers.flags) << ")\n";

    std::cout << "BC: 0x" << std::setw(4) << registers.bc << "   "
              << "(B: 0x" << std::setw(2) << static_cast<int>(registers.b)
              << ", C: 0x" << std::setw(2) << static_cast<int>(registers.c) << ")\n";

    std::cout << "DE: 0x" << std::setw(4) << registers.de << "   "
              << "(D: 0x" << std::setw(2) << static_cast<int>(registers.d)
              << ", E: 0x" << std::setw(2) << static_cast<int>(registers.e) << ")\n";

    std::cout << "HL: 0x" << std::setw(4) << registers.hl << "   "
              << "(H: 0x" << std::setw(2) << static_cast<int>(registers.h)
              << ", L: 0x" << std::setw(2) << static_cast<int>(registers.l) << ")\n";

    std::cout << "Stack Pointer: 0x" << std::setw(4) << registers.stack_pointer << "\n";
    std::cout << "Program Counter: 0x" << std::setw(4) << registers.program_counter << "\n";

    std::cout << std::dec;
    std::cout << "Cycles Elapsed: " << cycles_elapsed << "\n";
    std::cout << "===================================================\n";
}

void CPU::execute_instruction(uint8_t opcode) {
    if (opcode != 0xcb) {
        (this->*instruction_table[opcode])();
    } else {
        (this->*extended_instruction_table[memory.read_8(registers.program_counter + 1)])();
    }

    if (did_enable_interrupts_execute) {
        are_interrupts_enabled = true;
        did_enable_interrupts_execute = false;
    }
}

const CPU::Instruction CPU::instruction_table[256] = {
    &CPU::no_operation_0x00,
    &CPU::load_bc_immediate16_0x01,
    &CPU::load_memory_bc_a_0x02,
    &CPU::increment_bc_0x03,
    &CPU::increment_b_0x04,
    &CPU::decrement_b_0x05,
    &CPU::load_b_immediate8_0x06,
    &CPU::rotate_left_circular_a_0x07,
    &CPU::load_memory_immediate16_stack_pointer_0x08,
    &CPU::add_hl_bc_0x09,
    &CPU::load_a_memory_bc_0x0a,
    &CPU::decrement_bc_0x0b,
    &CPU::increment_c_0x0c,
    &CPU::decrement_c_0x0d,
    &CPU::load_c_immediate8_0x0e,
    &CPU::rotate_right_circular_a_0x0f,
    &CPU::stop_immediate8_0x10,
    &CPU::load_de_immediate16_0x11,
    &CPU::load_memory_de_a_0x12,
    &CPU::increment_de_0x13,
    &CPU::increment_d_0x14,
    &CPU::decrement_d_0x15,
    &CPU::load_d_immediate8_0x16,
    &CPU::rotate_left_through_carry_a_0x17,
    &CPU::jump_relative_signed_immediate8_0x18,
    &CPU::add_hl_de_0x19,
    &CPU::load_a_memory_de_0x1a,
    &CPU::decrement_de_0x1b,
    &CPU::increment_e_0x1c,
    &CPU::decrement_e_0x1d,
    &CPU::load_e_immediate8_0x1e,
    &CPU::rotate_right_through_carry_a_0x1f,
    &CPU::jump_relative_not_zero_signed_immediate8_0x20,
    &CPU::load_hl_immediate16_0x21,
    &CPU::load_memory_hli_a_0x22,
    &CPU::increment_hl_0x23,
    &CPU::increment_h_0x24,
    &CPU::decrement_h_0x25,
    &CPU::load_h_immediate8_0x26,
    &CPU::decimal_adjust_a_0x27,
    &CPU::jump_relative_zero_signed_immediate8_0x28,
    &CPU::add_hl_hl_0x29,
    &CPU::load_a_memory_hli_0x2a,
    &CPU::decrement_hl_0x2b,
    &CPU::increment_l_0x2c,
    &CPU::decrement_l_0x2d,
    &CPU::load_l_immediate8_0x2e,
    &CPU::complement_a_0x2f,
    &CPU::jump_relative_not_carry_signed_immediate8_0x30,
    &CPU::load_stack_pointer_immediate16_0x31,
    &CPU::load_memory_hload_a_0x32,
    &CPU::increment_stack_pointer_0x33,
    &CPU::increment_memory_hl_0x34,
    &CPU::decrement_memory_hl_0x35,
    &CPU::load_memory_hl_immediate8_0x36,
    &CPU::set_carry_flag_0x37,
    &CPU::jump_relative_carry_signed_immediate8_0x38,
    &CPU::add_hl_stack_pointer_0x39,
    &CPU::load_a_memory_hload_0x3a,
    &CPU::decrement_stack_pointer_0x3b,
    &CPU::increment_a_0x3c,
    &CPU::decrement_a_0x3d,
    &CPU::load_a_immediate8_0x3e,
    &CPU::complement_carry_flag_0x3f,
    &CPU::load_b_b_0x40,
    &CPU::load_b_c_0x41,
    &CPU::load_b_d_0x42,
    &CPU::load_b_e_0x43,
    &CPU::load_b_h_0x44,
    &CPU::load_b_l_0x45,
    &CPU::load_b_memory_hl_0x46,
    &CPU::load_b_a_0x47,
    &CPU::load_c_b_0x48,
    &CPU::load_c_c_0x49,
    &CPU::load_c_d_0x4a,
    &CPU::load_c_e_0x4b,
    &CPU::load_c_h_0x4c,
    &CPU::load_c_l_0x4d,
    &CPU::load_c_memory_hl_0x4e,
    &CPU::load_c_a_0x4f,
    &CPU::load_d_b_0x50,
    &CPU::load_d_c_0x51,
    &CPU::load_d_d_0x52,
    &CPU::load_d_e_0x53,
    &CPU::load_d_h_0x54,
    &CPU::load_d_l_0x55,
    &CPU::load_d_memory_hl_0x56,
    &CPU::load_d_a_0x57,
    &CPU::load_e_b_0x58,
    &CPU::load_e_c_0x59,
    &CPU::load_e_d_0x5a,
    &CPU::load_e_e_0x5b,
    &CPU::load_e_h_0x5c,
    &CPU::load_e_l_0x5d,
    &CPU::load_e_memory_hl_0x5e,
    &CPU::load_e_a_0x5f,
    &CPU::load_h_b_0x60,
    &CPU::load_h_c_0x61,
    &CPU::load_h_d_0x62,
    &CPU::load_h_e_0x63,
    &CPU::load_h_h_0x64,
    &CPU::load_h_l_0x65,
    &CPU::load_h_memory_hl_0x66,
    &CPU::load_h_a_0x67,
    &CPU::load_l_b_0x68,
    &CPU::load_l_c_0x69,
    &CPU::load_l_d_0x6a,
    &CPU::load_l_e_0x6b,
    &CPU::load_l_h_0x6c,
    &CPU::load_l_l_0x6d,
    &CPU::load_l_memory_hl_0x6e,
    &CPU::load_l_a_0x6f,
    &CPU::load_memory_hl_b_0x70,
    &CPU::load_memory_hl_c_0x71,
    &CPU::load_memory_hl_d_0x72,
    &CPU::load_memory_hl_e_0x73,
    &CPU::load_memory_hl_h_0x74,
    &CPU::load_memory_hl_l_0x75,
    &CPU::halt_0x76,
    &CPU::load_memory_hl_a_0x77,
    &CPU::load_a_b_0x78,
    &CPU::load_a_c_0x79,
    &CPU::load_a_d_0x7a,
    &CPU::load_a_e_0x7b,
    &CPU::load_a_h_0x7c,
    &CPU::load_a_l_0x7d,
    &CPU::load_a_memory_hl_0x7e,
    &CPU::load_a_a_0x7f,
    &CPU::add_a_b_0x80,
    &CPU::add_a_c_0x81,
    &CPU::add_a_d_0x82,
    &CPU::add_a_e_0x83,
    &CPU::add_a_h_0x84,
    &CPU::add_a_l_0x85,
    &CPU::add_a_memory_hl_0x86,
    &CPU::add_a_a_0x87,
    &CPU::add_with_carry_a_b_0x88,
    &CPU::add_with_carry_a_c_0x89,
    &CPU::add_with_carry_a_d_0x8a,
    &CPU::add_with_carry_a_e_0x8b,
    &CPU::add_with_carry_a_h_0x8c,
    &CPU::add_with_carry_a_l_0x8d,
    &CPU::add_with_carry_a_memory_hl_0x8e,
    &CPU::add_with_carry_a_a_0x8f,
    &CPU::subtract_a_b_0x90,
    &CPU::subtract_a_c_0x91,
    &CPU::subtract_a_d_0x92,
    &CPU::subtract_a_e_0x93,
    &CPU::subtract_a_h_0x94,
    &CPU::subtract_a_l_0x95,
    &CPU::subtract_a_memory_hl_0x96,
    &CPU::subtract_a_a_0x97,
    &CPU::subtract_with_carry_a_b_0x98,
    &CPU::subtract_with_carry_a_c_0x99,
    &CPU::subtract_with_carry_a_d_0x9a,
    &CPU::subtract_with_carry_a_e_0x9b,
    &CPU::subtract_with_carry_a_h_0x9c,
    &CPU::subtract_with_carry_a_l_0x9d,
    &CPU::subtract_with_carry_a_memory_hl_0x9e,
    &CPU::subtract_with_carry_a_a_0x9f,
    &CPU::and_a_b_0xa0,
    &CPU::and_a_c_0xa1,
    &CPU::and_a_d_0xa2,
    &CPU::and_a_e_0xa3,
    &CPU::and_a_h_0xa4,
    &CPU::and_a_l_0xa5,
    &CPU::and_a_memory_hl_0xa6,
    &CPU::and_a_a_0xa7,
    &CPU::xor_a_b_0xa8,
    &CPU::xor_a_c_0xa9,
    &CPU::xor_a_d_0xaa,
    &CPU::xor_a_e_0xab,
    &CPU::xor_a_h_0xac,
    &CPU::xor_a_l_0xad,
    &CPU::xor_a_memory_hl_0xae,
    &CPU::xor_a_a_0xaf,
    &CPU::or_a_b_0xb0,
    &CPU::or_a_c_0xb1,
    &CPU::or_a_d_0xb2,
    &CPU::or_a_e_0xb3,
    &CPU::or_a_h_0xb4,
    &CPU::or_a_l_0xb5,
    &CPU::or_a_memory_hl_0xb6,
    &CPU::or_a_a_0xb7,
    &CPU::compare_a_b_0xb8,
    &CPU::compare_a_c_0xb9,
    &CPU::compare_a_d_0xba,
    &CPU::compare_a_e_0xbb,
    &CPU::compare_a_h_0xbc,
    &CPU::compare_a_l_0xbd,
    &CPU::compare_a_memory_hl_0xbe,
    &CPU::compare_a_a_0xbf,
    &CPU::return_not_zero_0xc0,
    &CPU::pop_bc_0xc1,
    &CPU::jump_not_zero_immediate16_0xc2,
    &CPU::jump_immediate16_0xc3,
    &CPU::call_not_zero_immediate16_0xc4,
    &CPU::push_bc_0xc5,
    &CPU::add_a_immediate8_0xc6,
    &CPU::restart_0x00_0xc7,
    &CPU::return_zero_0xc8,
    &CPU::return_0xc9,
    &CPU::jump_zero_immediate16_0xca,
    &CPU::unused_opcode, // 0xcb is only used to prefix an extended instruction
    &CPU::call_zero_immediate16_0xcc,
    &CPU::call_immediate16_0xcd,
    &CPU::add_with_carry_a_immediate8_0xce,
    &CPU::restart_0x08_0xcf,
    &CPU::return_not_carry_0xd0,
    &CPU::pop_de_0xd1,
    &CPU::jump_not_carry_immediate16_0xd2,
    &CPU::unused_opcode, // 0xd3 is only used to prefix an extended instruction
    &CPU::call_not_carry_immediate16_0xd4,
    &CPU::push_de_0xd5,
    &CPU::subtract_a_immediate8_0xd6,
    &CPU::restart_0x10_0xd7,
    &CPU::return_carry_0xd8,
    &CPU::return_from_interrupt_0xd9,
    &CPU::jump_carry_immediate16_0xda,
    &CPU::unused_opcode, // 0xdb is only used to prefix an extended instruction
    &CPU::call_carry_immediate16_0xdc,
    &CPU::unused_opcode, // 0xdd is only used to prefix an extended instruction
    &CPU::subtract_with_carry_a_immediate8_0xde,
    &CPU::restart_0x18_0xdf,
    &CPU::load_memory_high_ram_signed_immediate8_a_0xe0,
    &CPU::pop_hl_0xe1,
    &CPU::load_memory_high_ram_c_a_0xe2,
    &CPU::unused_opcode,
    &CPU::unused_opcode,
    &CPU::push_hl_0xe5,
    &CPU::and_a_immediate8_0xe6,
    &CPU::restart_0x20_0xe7,
    &CPU::add_sp_signed_immediate8_0xe8,
    &CPU::jump_hl_0xe9,
    &CPU::load_memory_immediate16_a_0xea,
    &CPU::unused_opcode, // 0xeb is an unused opcode
    &CPU::unused_opcode, // 0xec is an unused opcode
    &CPU::unused_opcode, // 0xed is an unused opcode
    &CPU::xor_a_immediate8_0xee,
    &CPU::restart_0x28_0xef,
    &CPU::load_a_memory_high_ram_immediate8_0xf0,
    &CPU::pop_af_0xf1,
    &CPU::load_a_memory_high_ram_c_0xf2,
    &CPU::disable_interrupts_0xf3,
    &CPU::unused_opcode, // 0xf4 is an unused opcode
    &CPU::push_af_0xf5,
    &CPU::or_a_immediate8_0xf6,
    &CPU::restart_0x30_0xf7,
    &CPU::load_hl_stack_pointer_with_signed_offset_0xf8,
    &CPU::load_stack_pointer_hl_0xf9,
    &CPU::load_a_memory_immediate16_0xfa,
    &CPU::enable_interrupts_0xfb,
    &CPU::unused_opcode, // 0xfc is an unused opcode
    &CPU::unused_opcode, // 0xfd is an unused opcode
    &CPU::compare_a_immediate8_0xfe,
    &CPU::restart_0x38_0xff
};

const CPU::Instruction CPU::extended_instruction_table[256] = {
    // TODO implement remaining cases
};

// ===============================
// ===== Instruction Helpers =====
// ===============================

void CPU::set_flags(bool set_zero, bool set_subtract, bool set_half_carry, bool set_carry) {
    registers.flags = (set_zero ? MASK_ZERO : 0) |
                      (set_subtract ? MASK_SUBTRACT : 0) |
                      (set_half_carry ? MASK_HALF_CARRY : 0) |
                      (set_carry ? MASK_CARRY : 0);
}

void CPU::load_register8_register8(uint8_t &destination_register8, const uint8_t &source_register8) {
    destination_register8 = source_register8;
    registers.program_counter += 1;
    cycles_elapsed += 4;
}

void CPU::load_register8_immediate8(uint8_t &destination_register8) {
    destination_register8 = memory.read_8(registers.program_counter + 1);
    registers.program_counter += 2;
    cycles_elapsed += 8;
}

void CPU::load_register8_memory_register16(uint8_t &destination_register8, const uint16_t &source_address_register16) {
    destination_register8 = memory.read_8(source_address_register16);
    registers.program_counter += 1;
    cycles_elapsed += 8;
}

void CPU::load_memory_register16_register8(const uint16_t &destination_address_register16, const uint8_t &source_register8) {
    memory.write_8(destination_address_register16, source_register8);
    registers.program_counter += 1;
    cycles_elapsed += 8;
}

void CPU::load_register16_immediate16(uint16_t &destination_register16) {
    destination_register16 = memory.read_16(registers.program_counter + 1);
    registers.program_counter += 3;
    cycles_elapsed += 12;
}

void CPU::increment_register8(uint8_t &register8) {
    const bool does_half_carry_occur = (register8 & 0x0f) == 0x0f;
    register8++;
    set_flags(register8 == 0, false, does_half_carry_occur, registers.flags & MASK_CARRY);
    registers.program_counter += 1;
    cycles_elapsed += 4;
}

void CPU::decrement_register8(uint8_t &register8) {
    const bool does_half_carry_occur = (register8 & 0x0f) == 0x00;
    register8--;
    set_flags(register8 == 0, true, does_half_carry_occur, registers.flags & MASK_CARRY);
    registers.program_counter += 1;
    cycles_elapsed += 4;
}

void CPU::increment_register16(uint16_t &register16) {
    register16++;
    registers.program_counter += 1;
    cycles_elapsed += 8;
}

void CPU::decrement_register16(uint16_t &register16) {
    register16--;
    registers.program_counter += 1;
    cycles_elapsed += 8;
}

void CPU::add_hl_register16(const uint16_t &register16) {
    const bool does_half_carry_occur = (registers.hl & 0x0fff) + (register16 & 0x0fff) > 0x0fff;
    const bool does_carry_occur = static_cast<uint32_t>(registers.hl) + register16 > 0xffff;
    registers.hl += register16;
    set_flags(registers.flags & MASK_ZERO, false, does_half_carry_occur, does_carry_occur);
    registers.program_counter += 1;
    cycles_elapsed += 8;
}

void CPU::add_a_register8(const uint8_t &register8) {
    const bool does_half_carry_occur = (registers.a & 0x0f) + (register8 & 0x0f) > 0x0f;
    const bool does_carry_occur = static_cast<uint16_t>(registers.a) + register8 > 0xff;
    registers.a += register8;
    set_flags(registers.a == 0, false, does_half_carry_occur, does_carry_occur);
    registers.program_counter += 1;
    cycles_elapsed += 4;
}

void CPU::add_with_carry_a_register8(const uint8_t &register8) {
    const auto carry_in = (registers.flags & MASK_CARRY) ? 1 : 0;
    const bool does_half_carry_occur = (registers.a & 0x0f) + (register8 & 0x0f) + carry_in > 0x0f;
    const bool does_carry_occur = static_cast<uint16_t>(registers.a) + register8 + carry_in > 0xff;
    registers.a += register8 + carry_in;
    set_flags(registers.a == 0, false, does_half_carry_occur, does_carry_occur);
    registers.program_counter += 1;
    cycles_elapsed += 4;
}

void CPU::subtract_a_register8(const uint8_t &register8) {
    const bool does_half_carry_occur = (registers.a & 0x0f) < (register8 & 0x0f);
    const bool does_carry_occur = registers.a < register8;
    registers.a -= register8;
    set_flags(registers.a == 0, true, does_half_carry_occur, does_carry_occur);
    registers.program_counter += 1;
    cycles_elapsed += 4;
}

void CPU::subtract_with_carry_a_register8(const uint8_t &register8) {
    const auto carry_in = (registers.flags & MASK_CARRY) ? 1 : 0;
    const bool does_half_carry_occur = (registers.a & 0x0f) < (register8 & 0x0f) + carry_in;
    const bool does_carry_occur = registers.a < register8 + carry_in;
    registers.a -= register8 + carry_in;
    set_flags(registers.a == 0, true, does_half_carry_occur, does_carry_occur);
    registers.program_counter += 1;
    cycles_elapsed += 4;
}

void CPU::and_a_register8(const uint8_t &register8) {
    registers.a &= register8;
    set_flags(registers.a == 0, false, true, false);
    registers.program_counter += 1;
    cycles_elapsed += 4;
}

void CPU::xor_a_register8(const uint8_t &register8) {
    registers.a ^= register8;
    set_flags(registers.a == 0, false, false, false);
    registers.program_counter += 1;
    cycles_elapsed += 4;
}

void CPU::or_a_register8(const uint8_t &register8) {
    registers.a |= register8;
    set_flags(registers.a == 0, false, false, false);
    registers.program_counter += 1;
    cycles_elapsed += 4;
}

void CPU::compare_a_register8(const uint8_t &register8) {
    const bool does_half_carry_occur = (registers.a & 0x0f) < (register8 & 0x0f);
    const bool does_carry_occur = registers.a < register8;
    set_flags(registers.a == register8, true, does_half_carry_occur, does_carry_occur);
    registers.program_counter += 1;
    cycles_elapsed += 4;
}

void CPU::jump_relative_condition_signed_immediate8(bool is_condition_met) {
    if (is_condition_met) {
        const auto signed_offset = static_cast<int8_t>(memory.read_8(registers.program_counter + 1));
        registers.program_counter += 2 + signed_offset;
        cycles_elapsed += 12;
    } else {
        registers.program_counter += 2;
        cycles_elapsed += 8;
    }
}

void CPU::jump_condition_immediate16(bool is_condition_met) {
    if (is_condition_met) {
        registers.program_counter = memory.read_16(registers.program_counter + 1);
        cycles_elapsed += 16;
    } else {
        registers.program_counter += 3;
        cycles_elapsed += 12;
    }
}

void CPU::call_condition_immediate16(bool is_condition_met) {
    if (is_condition_met) {
        const uint16_t return_address = registers.program_counter + 3;
        registers.stack_pointer -= 2;
        memory.write_16(registers.stack_pointer, return_address);
        registers.program_counter = memory.read_16(registers.program_counter + 1);
        cycles_elapsed += 24;
    } else {
        registers.program_counter += 3;
        cycles_elapsed += 12;
    }
}

void CPU::return_condition(bool is_condition_met) {
    if (is_condition_met) {
        registers.program_counter = memory.read_16(registers.stack_pointer);
        registers.stack_pointer += 2;
        cycles_elapsed += 20;
    } else {
        registers.program_counter += 1;
        cycles_elapsed += 8;
    }
}

void CPU::push_register16(const uint16_t &register16) {
    registers.stack_pointer -= 2;
    memory.write_16(registers.stack_pointer, register16);
    registers.program_counter += 1;
    cycles_elapsed += 16;
}

void CPU::pop_register16(uint16_t &register16) {
    register16 = memory.read_16(registers.stack_pointer);
    registers.stack_pointer += 2;
    registers.program_counter += 1;
    cycles_elapsed += 12;
}

void CPU::restart_address(uint16_t address) {
    const uint16_t return_address = registers.program_counter + 1;
    registers.stack_pointer -= 2;
    memory.write_16(registers.stack_pointer, return_address);
    registers.program_counter = address;
    cycles_elapsed += 16;
}

// ========================
// ===== Instructions =====
// ========================

void CPU::unused_opcode() {
    std::cerr << std::hex << std::setfill('0');
    std::cerr << "Unused opcode 0x" << std::setw(2) << memory.read_8(registers.program_counter) << " "
              << "encountered at registers.program_counter = 0x" << std::setw(2) << registers.program_counter << "\n";
}

void CPU::no_operation_0x00() {
    registers.program_counter += 1;
    cycles_elapsed += 4;
}

void CPU::load_bc_immediate16_0x01() {
    load_register16_immediate16(registers.bc);
}

void CPU::load_memory_bc_a_0x02() {
    load_memory_register16_register8(registers.bc, registers.a);
}

void CPU::increment_bc_0x03() {
    increment_register16(registers.bc);
}

void CPU::increment_b_0x04() {
    increment_register8(registers.b);
}

void CPU::decrement_b_0x05() {
    decrement_register8(registers.b);
}

void CPU::load_b_immediate8_0x06() {
    load_register8_immediate8(registers.b);
}

void CPU::rotate_left_circular_a_0x07() {
    const bool does_carry_occur = (registers.a & 0b10000000) == 0b10000000;
    registers.a = (registers.a << 1) | (registers.a >> 7);
    set_flags(false, false, false, does_carry_occur);
    registers.program_counter += 1;
    cycles_elapsed += 4;
}

void CPU::load_memory_immediate16_stack_pointer_0x08() {
    const auto immediate16 = memory.read_16(registers.program_counter + 1);
    memory.write_16(immediate16, registers.stack_pointer);
    registers.program_counter += 3;
    cycles_elapsed += 20;
}

void CPU::add_hl_bc_0x09() {
    add_hl_register16(registers.bc);
}

void CPU::load_a_memory_bc_0x0a() {
    load_register8_memory_register16(registers.a, registers.bc);
}

void CPU::decrement_bc_0x0b() {
    decrement_register16(registers.bc);
}

void CPU::increment_c_0x0c() {
    increment_register8(registers.c);
}

void CPU::decrement_c_0x0d() {
    decrement_register8(registers.c);
}

void CPU::load_c_immediate8_0x0e() {
    load_register8_immediate8(registers.c);
}

void CPU::rotate_right_circular_a_0x0f() {
    const bool does_carry_occur = (registers.a & 0b00000001) == 0b00000001;
    registers.a = (registers.a >> 1) | (registers.a << 7);
    set_flags(false, false, false, does_carry_occur);
    registers.program_counter += 1;
    cycles_elapsed += 4;
}

void CPU::stop_immediate8_0x10() {
    is_stopped = true;
    registers.program_counter += 2; // Immediate isn't actually read - it's skipped over and usually will be 0x00
    cycles_elapsed += 4;
}

void CPU::load_de_immediate16_0x11() {
    load_register16_immediate16(registers.de);
}

void CPU::load_memory_de_a_0x12() {
    load_memory_register16_register8(registers.de, registers.a);
}

void CPU::increment_de_0x13() {
    increment_register16(registers.de);
}

void CPU::increment_d_0x14() {
    increment_register8(registers.d);
}

void CPU::decrement_d_0x15() {
    decrement_register8(registers.d);
}

void CPU::load_d_immediate8_0x16() {
    load_register8_immediate8(registers.d);
}

void CPU::rotate_left_through_carry_a_0x17() {
    const bool does_carry_occur = (registers.a & 0b10000000) == 0b10000000;
    registers.a = (registers.a << 1) | ((registers.flags & MASK_CARRY) >> 4);
    set_flags(false, false, false, does_carry_occur);
    registers.program_counter += 1;
    cycles_elapsed += 4;
}

void CPU::jump_relative_signed_immediate8_0x18() {
    jump_relative_condition_signed_immediate8(true);
}

void CPU::add_hl_de_0x19() {
    add_hl_register16(registers.de);
}

void CPU::load_a_memory_de_0x1a() {
    load_register8_memory_register16(registers.a, registers.de);
}

void CPU::decrement_de_0x1b() {
    decrement_register16(registers.de);
}

void CPU::increment_e_0x1c() {
    increment_register8(registers.e);
}

void CPU::decrement_e_0x1d() {
    decrement_register8(registers.e);
}

void CPU::load_e_immediate8_0x1e() {
    load_register8_immediate8(registers.e);
}

void CPU::rotate_right_through_carry_a_0x1f() {
    const bool does_carry_occur = (registers.a & 0b00000001) == 0b00000001;
    registers.a = (registers.a >> 1) | ((registers.flags & MASK_CARRY) << 3);
    set_flags(false, false, false, does_carry_occur);
    registers.program_counter += 1;
    cycles_elapsed += 4;
}

void CPU::jump_relative_not_zero_signed_immediate8_0x20() {
    jump_relative_condition_signed_immediate8(!(registers.flags & MASK_ZERO));
}

void CPU::load_hl_immediate16_0x21() {
    load_register16_immediate16(registers.hl);
}

void CPU::load_memory_hli_a_0x22() {
    load_memory_register16_register8(registers.hl++, registers.a);
}

void CPU::increment_hl_0x23() {
    increment_register16(registers.hl);
}

void CPU::increment_h_0x24() {
    increment_register8(registers.h);
}

void CPU::decrement_h_0x25() {
    decrement_register8(registers.h);
}

void CPU::load_h_immediate8_0x26() {
    load_register8_immediate8(registers.h);
}

void CPU::decimal_adjust_a_0x27() {
    const bool was_addition_most_recent = !(registers.flags & MASK_SUBTRACT);
    bool does_carry_occur = false;
    uint8_t correction = 0;
    // Previous operation was between two binary coded decimals (BCDs) and this corrects register A back to BCD format
    if ((registers.flags & MASK_HALF_CARRY) || (was_addition_most_recent && (registers.a & 0x0f) > 0x09)) {
        correction |= 0x06;
    }
    if ((registers.flags & MASK_CARRY) || (was_addition_most_recent && registers.a > 0x99)) {
        correction |= 0x60;
        does_carry_occur = was_addition_most_recent;
    }
    registers.a = was_addition_most_recent ? (registers.a + correction) : (registers.a - correction);
    set_flags(registers.a == 0, registers.flags & MASK_SUBTRACT, false, does_carry_occur);
    registers.program_counter += 1;
    cycles_elapsed += 4;
}

void CPU::jump_relative_zero_signed_immediate8_0x28() {
    jump_relative_condition_signed_immediate8(registers.flags & MASK_ZERO);
}

void CPU::add_hl_hl_0x29() {
    add_hl_register16(registers.hl);
}

void CPU::load_a_memory_hli_0x2a() {
    load_register8_memory_register16(registers.a, registers.hl++);
}

void CPU::decrement_hl_0x2b() {
    decrement_register16(registers.hl);
}

void CPU::increment_l_0x2c() {
    increment_register8(registers.l);
}

void CPU::decrement_l_0x2d() {
    decrement_register8(registers.l);
}

void CPU::load_l_immediate8_0x2e() {
    load_register8_immediate8(registers.l);
}

void CPU::complement_a_0x2f() {
    registers.a = ~registers.a;
    set_flags(registers.flags & MASK_ZERO, true, true, registers.flags & MASK_HALF_CARRY);
    registers.program_counter += 1;
    cycles_elapsed += 4;
}

void CPU::jump_relative_not_carry_signed_immediate8_0x30() {
    jump_relative_condition_signed_immediate8(!(registers.flags & MASK_CARRY));
}

void CPU::load_stack_pointer_immediate16_0x31() {
    load_register16_immediate16(registers.stack_pointer);
}

void CPU::load_memory_hload_a_0x32() {
    load_memory_register16_register8(registers.hl--, registers.a);
}

void CPU::increment_stack_pointer_0x33() {
    increment_register16(registers.stack_pointer);
}

void CPU::increment_memory_hl_0x34() {
    auto memory_hl = memory.read_8(registers.hl);
    const bool does_half_carry_occur = (memory_hl & 0x0f) == 0x0f;
    memory.write_8(registers.hl, ++memory_hl);
    set_flags(memory_hl == 0, false, does_half_carry_occur, registers.flags & MASK_CARRY);
    registers.program_counter += 1;
    cycles_elapsed += 12;
}

void CPU::decrement_memory_hl_0x35() {
    auto memory_hl = memory.read_8(registers.hl);
    const bool does_half_carry_occur = (memory_hl & 0x0f) == 0x00;
    memory.write_8(registers.hl, --memory_hl);
    set_flags(memory_hl == 0, true, does_half_carry_occur, registers.flags & MASK_CARRY);
    registers.program_counter += 1;
    cycles_elapsed += 12;
}

void CPU::load_memory_hl_immediate8_0x36() {
    memory.write_8(registers.hl, memory.read_8(registers.program_counter + 1));
    registers.program_counter += 2;
    cycles_elapsed += 12;
}

void CPU::set_carry_flag_0x37() {
    set_flags(registers.flags & MASK_ZERO, false, false, true);
    registers.program_counter += 1;
    cycles_elapsed += 4;
}

void CPU::jump_relative_carry_signed_immediate8_0x38() {
    jump_relative_condition_signed_immediate8(registers.flags & MASK_CARRY);
}

void CPU::add_hl_stack_pointer_0x39() {
    add_hl_register16(registers.stack_pointer);
}

void CPU::load_a_memory_hload_0x3a() {
    load_register8_memory_register16(registers.a, registers.hl--);
}

void CPU::decrement_stack_pointer_0x3b() {
    decrement_register16(registers.stack_pointer);
}

void CPU::increment_a_0x3c() {
    increment_register8(registers.a);
}

void CPU::decrement_a_0x3d() {
    decrement_register8(registers.a);
}

void CPU::load_a_immediate8_0x3e() {
    load_register8_immediate8(registers.a);
}

void CPU::complement_carry_flag_0x3f() {
    set_flags(registers.flags & MASK_ZERO, false, false, !(registers.flags & MASK_CARRY));
    registers.program_counter += 1;
    cycles_elapsed += 4;
}

void CPU::load_b_b_0x40() {
    load_register8_register8(registers.b, registers.b); // No effect, but still advances pc/cycles_elapsed
}

void CPU::load_b_c_0x41() {
    load_register8_register8(registers.b, registers.c);
}

void CPU::load_b_d_0x42() {
    load_register8_register8(registers.b, registers.d);
}

void CPU::load_b_e_0x43() {
    load_register8_register8(registers.b, registers.e);
}

void CPU::load_b_h_0x44() {
    load_register8_register8(registers.b, registers.h);
}

void CPU::load_b_l_0x45() {
    load_register8_register8(registers.b, registers.l);
}

void CPU::load_b_memory_hl_0x46() {
    load_register8_memory_register16(registers.b, registers.hl);
}

void CPU::load_b_a_0x47() {
    load_register8_register8(registers.b, registers.a);
}

void CPU::load_c_b_0x48() {
    load_register8_register8(registers.c, registers.b);
}

void CPU::load_c_c_0x49() {
    load_register8_register8(registers.c, registers.c); // No effect, but still advances pc/cycles_elapsed
}

void CPU::load_c_d_0x4a() {
    load_register8_register8(registers.c, registers.d);
}

void CPU::load_c_e_0x4b() {
    load_register8_register8(registers.c, registers.e);
}

void CPU::load_c_h_0x4c() {
    load_register8_register8(registers.c, registers.h);
}

void CPU::load_c_l_0x4d() {
    load_register8_register8(registers.c, registers.l);
}

void CPU::load_c_memory_hl_0x4e() {
    load_register8_memory_register16(registers.c, registers.hl);
}

void CPU::load_c_a_0x4f() {
    load_register8_register8(registers.c, registers.a);
}

void CPU::load_d_b_0x50() {
    load_register8_register8(registers.d, registers.b);
}

void CPU::load_d_c_0x51() {
    load_register8_register8(registers.d, registers.c);
}

void CPU::load_d_d_0x52() {
    load_register8_register8(registers.d, registers.d); // No effect, but still advances pc/cycles_elapsed
}

void CPU::load_d_e_0x53() {
    load_register8_register8(registers.d, registers.e);
}

void CPU::load_d_h_0x54() {
    load_register8_register8(registers.d, registers.h);
}

void CPU::load_d_l_0x55() {
    load_register8_register8(registers.d, registers.l);
}

void CPU::load_d_memory_hl_0x56() {
    load_register8_memory_register16(registers.d, registers.hl);
}

void CPU::load_d_a_0x57() {
    load_register8_register8(registers.d, registers.a);
}

void CPU::load_e_b_0x58() {
    load_register8_register8(registers.e, registers.b);
}

void CPU::load_e_c_0x59() {
    load_register8_register8(registers.e, registers.c);
}

void CPU::load_e_d_0x5a() {
    load_register8_register8(registers.e, registers.d);
}

void CPU::load_e_e_0x5b() {
    load_register8_register8(registers.e, registers.e); // No effect, but still advances pc/cycles_elapsed
}

void CPU::load_e_h_0x5c() {
    load_register8_register8(registers.e, registers.h);
}

void CPU::load_e_l_0x5d() {
    load_register8_register8(registers.e, registers.l);
}

void CPU::load_e_memory_hl_0x5e() {
    load_register8_memory_register16(registers.e, registers.hl);
}

void CPU::load_e_a_0x5f() {
    load_register8_register8(registers.e, registers.a);
}

void CPU::load_h_b_0x60() {
    load_register8_register8(registers.h, registers.b);
}

void CPU::load_h_c_0x61() {
    load_register8_register8(registers.h, registers.c);
}

void CPU::load_h_d_0x62() {
    load_register8_register8(registers.h, registers.d);
}

void CPU::load_h_e_0x63() {
    load_register8_register8(registers.h, registers.e);
}

void CPU::load_h_h_0x64() {
    load_register8_register8(registers.h, registers.h);  // No effect, but still advances pc/cycles_elapsed
}

void CPU::load_h_l_0x65() {
    load_register8_register8(registers.h, registers.l);
}

void CPU::load_h_memory_hl_0x66() {
    load_register8_memory_register16(registers.h, registers.hl);
}

void CPU::load_h_a_0x67() {
    load_register8_register8(registers.h, registers.a);
}

void CPU::load_l_b_0x68() {
    load_register8_register8(registers.l, registers.b);
}

void CPU::load_l_c_0x69() {
    load_register8_register8(registers.l, registers.c);
}

void CPU::load_l_d_0x6a() {
    load_register8_register8(registers.l, registers.d);
}

void CPU::load_l_e_0x6b() {
    load_register8_register8(registers.l, registers.e);
}

void CPU::load_l_h_0x6c() {
    load_register8_register8(registers.l, registers.h);
}

void CPU::load_l_l_0x6d() {
    load_register8_register8(registers.l, registers.l);  // No effect, but still advances pc/cycles_elapsed
}

void CPU::load_l_memory_hl_0x6e() {
    load_register8_memory_register16(registers.l, registers.hl);
}

void CPU::load_l_a_0x6f() {
    load_register8_register8(registers.l, registers.a);
}

void CPU::load_memory_hl_b_0x70() {
    load_memory_register16_register8(registers.hl, registers.b);
}

void CPU::load_memory_hl_c_0x71() {
    load_memory_register16_register8(registers.hl, registers.c);
}

void CPU::load_memory_hl_d_0x72() {
    load_memory_register16_register8(registers.hl, registers.d);
}

void CPU::load_memory_hl_e_0x73() {
    load_memory_register16_register8(registers.hl, registers.e);
}

void CPU::load_memory_hl_h_0x74() { 
    load_memory_register16_register8(registers.hl, registers.h);
}

void CPU::load_memory_hl_l_0x75() {
    load_memory_register16_register8(registers.hl, registers.l);
}

void CPU::halt_0x76() {
    is_halted = true;
    registers.program_counter += 1;
    cycles_elapsed += 4;
}

void CPU::load_memory_hl_a_0x77() {
    load_memory_register16_register8(registers.hl, registers.a);
}

void CPU::load_a_b_0x78() {
    load_register8_register8(registers.a, registers.b);
}

void CPU::load_a_c_0x79() {
    load_register8_register8(registers.a, registers.c);
}

void CPU::load_a_d_0x7a() {
    load_register8_register8(registers.a, registers.d);
}

void CPU::load_a_e_0x7b() {
    load_register8_register8(registers.a, registers.e);
}

void CPU::load_a_h_0x7c() {
    load_register8_register8(registers.a, registers.h);
}

void CPU::load_a_l_0x7d() {
    load_register8_register8(registers.a, registers.l);
}

void CPU::load_a_memory_hl_0x7e() {
    load_register8_memory_register16(registers.a, registers.hl);
}

void CPU::load_a_a_0x7f() {
    load_register8_register8(registers.a, registers.a); // No effect, but still advances pc/cycles_elapsed
}

void CPU::add_a_b_0x80() {
    add_a_register8(registers.b);
}

void CPU::add_a_c_0x81() {
    add_a_register8(registers.c);
}

void CPU::add_a_d_0x82() {
    add_a_register8(registers.d);
}

void CPU::add_a_e_0x83() {
    add_a_register8(registers.e);
}

void CPU::add_a_h_0x84() {
    add_a_register8(registers.h);
}

void CPU::add_a_l_0x85() {
    add_a_register8(registers.l);
}

void CPU::add_a_memory_hl_0x86() {
    const auto memory_hl = memory.read_8(registers.hl);
    const bool does_half_carry_occur = (registers.a & 0x0f) + (memory_hl & 0x0f) > 0x0f;
    const bool does_carry_occur = static_cast<uint16_t>(registers.a) + memory_hl > 0xff;
    registers.a += memory_hl;
    set_flags(registers.a == 0, false, does_half_carry_occur, does_carry_occur);
    registers.program_counter += 1;
    cycles_elapsed += 8;
}

void CPU::add_a_a_0x87() {
    add_a_register8(registers.a);
}

void CPU::add_with_carry_a_b_0x88() {
    add_with_carry_a_register8(registers.b);
}

void CPU::add_with_carry_a_c_0x89() {
    add_with_carry_a_register8(registers.c);
}

void CPU::add_with_carry_a_d_0x8a() {
    add_with_carry_a_register8(registers.d);
}

void CPU::add_with_carry_a_e_0x8b() {
    add_with_carry_a_register8(registers.e);
}

void CPU::add_with_carry_a_h_0x8c() {
    add_with_carry_a_register8(registers.h);
}

void CPU::add_with_carry_a_l_0x8d() {
    add_with_carry_a_register8(registers.l);
}

void CPU::add_with_carry_a_memory_hl_0x8e() {
    const auto memory_hl = memory.read_8(registers.hl);
    const auto carry_in = (registers.flags & MASK_CARRY) ? 1 : 0;
    const bool does_half_carry_occur = (registers.a & 0x0f) + (memory_hl & 0x0f) + carry_in > 0x0f;
    const bool does_carry_occur = static_cast<uint16_t>(registers.a) + memory_hl + carry_in > 0xff;
    registers.a += memory_hl + carry_in;
    set_flags(registers.a == 0, false, does_half_carry_occur, does_carry_occur);
    registers.program_counter += 1;
    cycles_elapsed += 8;
}

void CPU::add_with_carry_a_a_0x8f() {
    add_with_carry_a_register8(registers.a);
}

void CPU::subtract_a_b_0x90() {
    subtract_a_register8(registers.b);
}

void CPU::subtract_a_c_0x91() {
    subtract_a_register8(registers.c);
}

void CPU::subtract_a_d_0x92() {
    subtract_a_register8(registers.d);
}

void CPU::subtract_a_e_0x93() {
    subtract_a_register8(registers.e);
}

void CPU::subtract_a_h_0x94() {
    subtract_a_register8(registers.h);
}

void CPU::subtract_a_l_0x95() {
    subtract_a_register8(registers.l);
}

void CPU::subtract_a_memory_hl_0x96() {
    const auto memory_hl = memory.read_8(registers.hl);
    const bool does_half_carry_occur = (registers.a & 0x0f) < (memory_hl & 0x0f);
    const bool does_carry_occur = registers.a < memory_hl;
    registers.a -= memory_hl;
    set_flags(registers.a == 0, true, does_half_carry_occur, does_carry_occur);
    registers.program_counter += 1;
    cycles_elapsed += 8;
}

void CPU::subtract_a_a_0x97() {
    subtract_a_register8(registers.a);
}

void CPU::subtract_with_carry_a_b_0x98() {
    subtract_with_carry_a_register8(registers.b);
}

void CPU::subtract_with_carry_a_c_0x99() {
    subtract_with_carry_a_register8(registers.c);
}

void CPU::subtract_with_carry_a_d_0x9a() {
    subtract_with_carry_a_register8(registers.d);
}

void CPU::subtract_with_carry_a_e_0x9b() {
    subtract_with_carry_a_register8(registers.e);
}

void CPU::subtract_with_carry_a_h_0x9c() {
    subtract_with_carry_a_register8(registers.h);
}

void CPU::subtract_with_carry_a_l_0x9d() {
    subtract_with_carry_a_register8(registers.l);
}

void CPU::subtract_with_carry_a_memory_hl_0x9e() {
    const auto memory_hl = memory.read_8(registers.hl);
    const auto carry_in = (registers.flags & MASK_CARRY) ? 1 : 0;
    const bool does_half_carry_occur = (registers.a & 0x0f) < (memory_hl & 0x0f) + carry_in;
    const bool does_carry_occur = registers.a < memory_hl + carry_in;
    registers.a -= memory_hl + carry_in;
    set_flags(registers.a == 0, true, does_half_carry_occur, does_carry_occur);
    registers.program_counter += 1;
    cycles_elapsed += 8;
}

void CPU::subtract_with_carry_a_a_0x9f() {
    subtract_with_carry_a_register8(registers.a);
}

void CPU::and_a_b_0xa0() {
    and_a_register8(registers.b);
}

void CPU::and_a_c_0xa1() {
    and_a_register8(registers.c);
}

void CPU::and_a_d_0xa2() {
    and_a_register8(registers.d);
 }

void CPU::and_a_e_0xa3() {
    and_a_register8(registers.e);
}

void CPU::and_a_h_0xa4() {
    and_a_register8(registers.h);
}

void CPU::and_a_l_0xa5() {
    and_a_register8(registers.l);
}

void CPU::and_a_memory_hl_0xa6() {
    registers.a &= memory.read_8(registers.hl);
    set_flags(registers.a == 0, false, true, false);
    registers.program_counter += 1;
    cycles_elapsed += 8;
}

void CPU::and_a_a_0xa7() {
    and_a_register8(registers.a);
}

void CPU::xor_a_b_0xa8() {
    xor_a_register8(registers.b);
}

void CPU::xor_a_c_0xa9() {
    xor_a_register8(registers.c);
}

void CPU::xor_a_d_0xaa() {
    xor_a_register8(registers.d);
}

void CPU::xor_a_e_0xab() {
    xor_a_register8(registers.e);
}

void CPU::xor_a_h_0xac() {
    xor_a_register8(registers.h);
}

void CPU::xor_a_l_0xad() {
    xor_a_register8(registers.l);
}

void CPU::xor_a_memory_hl_0xae() {
    registers.a ^= memory.read_8(registers.hl);
    set_flags(registers.a == 0, false, false, false);
    registers.program_counter += 1;
    cycles_elapsed += 8;
}

void CPU::xor_a_a_0xaf() {
    xor_a_register8(registers.a);
}

void CPU::or_a_b_0xb0() {
    or_a_register8(registers.b);
}

void CPU::or_a_c_0xb1() {
    or_a_register8(registers.c);
}

void CPU::or_a_d_0xb2() {
    or_a_register8(registers.d);
}

void CPU::or_a_e_0xb3() {
    or_a_register8(registers.e);
}

void CPU::or_a_h_0xb4() {
    or_a_register8(registers.h);
}

void CPU::or_a_l_0xb5() {
    or_a_register8(registers.l);
}

void CPU::or_a_memory_hl_0xb6() {
    registers.a |= memory.read_8(registers.hl);
    set_flags(registers.a == 0, false, false, false);
    registers.program_counter += 1;
    cycles_elapsed += 8;
}

void CPU::or_a_a_0xb7() {
    or_a_register8(registers.a);
}

void CPU::compare_a_b_0xb8() {
    compare_a_register8(registers.b);
}

void CPU::compare_a_c_0xb9() {
    compare_a_register8(registers.c);
}

void CPU::compare_a_d_0xba() {
    compare_a_register8(registers.d);
}

void CPU::compare_a_e_0xbb() {
    compare_a_register8(registers.e);
}

void CPU::compare_a_h_0xbc() {
    compare_a_register8(registers.h);
}

void CPU::compare_a_l_0xbd() {
    compare_a_register8(registers.l);
}

void CPU::compare_a_memory_hl_0xbe() {
    const auto memory_hl = memory.read_8(registers.hl);
    const bool does_half_carry_occur = (registers.a & 0x0f) < (memory_hl & 0x0f);
    const bool does_carry_occur = registers.a < memory_hl;
    set_flags(registers.a == memory_hl, true, does_half_carry_occur, does_carry_occur);
    registers.program_counter += 1;
    cycles_elapsed += 8;
}

void CPU::compare_a_a_0xbf() {
    compare_a_register8(registers.a);
}

void CPU::return_not_zero_0xc0() {
    return_condition(!(registers.flags & MASK_ZERO));
}

void CPU::pop_bc_0xc1() {
    pop_register16(registers.bc);
}

void CPU::jump_not_zero_immediate16_0xc2() {
    jump_condition_immediate16(!(registers.flags & MASK_ZERO));
}

void CPU::jump_immediate16_0xc3() {
    jump_condition_immediate16(true);
}

void CPU::call_not_zero_immediate16_0xc4() {
    call_condition_immediate16(!(registers.flags & MASK_ZERO));
}

void CPU::push_bc_0xc5() {
    push_register16(registers.bc);
}

void CPU::add_a_immediate8_0xc6() {
    const auto immediate8 = memory.read_8(registers.program_counter + 1);
    const bool does_half_carry_occur = (registers.a & 0x0f) + (immediate8 & 0x0f) > 0x0f;
    const bool does_carry_occur = static_cast<uint16_t>(registers.a) + immediate8 > 0xff;
    registers.a += immediate8;
    set_flags(registers.a == 0, false, does_half_carry_occur, does_carry_occur);
    registers.program_counter += 2;
    cycles_elapsed += 8;
}

void CPU::restart_0x00_0xc7() {
    restart_address(0x00);
}

void CPU::return_zero_0xc8() {
    return_condition(registers.flags & MASK_ZERO);
}

void CPU::return_0xc9() {
    registers.program_counter = memory.read_16(registers.stack_pointer);
    registers.stack_pointer += 2;
    cycles_elapsed += 16;
}

void CPU::jump_zero_immediate16_0xca() {
    jump_condition_immediate16(registers.flags & MASK_ZERO);
}

// 0xcb is only used to prefix an extended instruction

void CPU::call_zero_immediate16_0xcc() {
    call_condition_immediate16(registers.flags & MASK_ZERO);
}

void CPU::call_immediate16_0xcd() {
    call_condition_immediate16(true);
}

void CPU::add_with_carry_a_immediate8_0xce() {
    const auto immediate8 = memory.read_8(registers.program_counter + 1);
    const auto carry_in = (registers.flags & MASK_CARRY) ? 1 : 0;
    const bool does_half_carry_occur = (registers.a & 0x0f) + (immediate8 & 0x0f) + carry_in > 0x0f;
    const bool does_carry_occur = static_cast<uint16_t>(registers.a) + immediate8 + carry_in > 0xff;
    registers.a += immediate8 + carry_in;
    set_flags(registers.a == 0, false, does_half_carry_occur, does_carry_occur);
    registers.program_counter += 2;
    cycles_elapsed += 8;
}

void CPU::restart_0x08_0xcf() {
    restart_address(0x08);
}

void CPU::return_not_carry_0xd0() {
    return_condition(!(registers.flags & MASK_CARRY));
}

void CPU::pop_de_0xd1() {
    pop_register16(registers.de);
}

void CPU::jump_not_carry_immediate16_0xd2() {
    jump_condition_immediate16(!(registers.flags & MASK_CARRY));
}

// 0xd3 is an unused opcode

void CPU::call_not_carry_immediate16_0xd4() {
    call_condition_immediate16(!(registers.flags & MASK_CARRY));
}

void CPU::push_de_0xd5() {
    push_register16(registers.de);
}

void CPU::subtract_a_immediate8_0xd6() {
    const auto immediate8 = memory.read_8(registers.program_counter + 1);
    const bool does_half_carry_occur = (registers.a & 0x0f) < (immediate8 & 0x0f);
    const bool does_carry_occur = registers.a < immediate8;
    registers.a -= immediate8;
    set_flags(registers.a == 0, true, does_half_carry_occur, does_carry_occur);
    registers.program_counter += 2;
    cycles_elapsed += 8;
}

void CPU::restart_0x10_0xd7() {
    restart_address(0x10);
}

void CPU::return_carry_0xd8() {
    return_condition(registers.flags & MASK_CARRY);
}

void CPU::return_from_interrupt_0xd9() {
    registers.program_counter = memory.read_16(registers.stack_pointer);
    registers.stack_pointer += 2;
    are_interrupts_enabled = true;
    cycles_elapsed += 16;
}

void CPU::jump_carry_immediate16_0xda() {
    jump_condition_immediate16(registers.flags & MASK_CARRY);
}

// 0xdb is an unused opcode

void CPU::call_carry_immediate16_0xdc() {
    call_condition_immediate16(registers.flags & MASK_CARRY);
}

// 0xdd is an unused opcode

void CPU::subtract_with_carry_a_immediate8_0xde() {
    const auto immediate8 = memory.read_8(registers.program_counter + 1);
    const auto carry_in = (registers.flags & MASK_CARRY) ? 1 : 0;
    const bool does_half_carry_occur = (registers.a & 0x0f) < (immediate8 & 0x0f) + carry_in;
    const bool does_carry_occur = registers.a < immediate8 + carry_in;
    registers.a -= immediate8 + carry_in;
    set_flags(registers.a == 0, true, does_half_carry_occur, does_carry_occur);
    registers.program_counter += 2;
    cycles_elapsed += 8;
}

void CPU::restart_0x18_0xdf() {
    restart_address(0x18);
}

void CPU::load_memory_high_ram_signed_immediate8_a_0xe0() {
    const auto signed_offset = static_cast<uint8_t>(memory.read_8(registers.program_counter + 1));
    memory.write_8(HIGH_RAM_START + signed_offset, registers.a);
    registers.program_counter += 2;
    cycles_elapsed += 12;
}

void CPU::pop_hl_0xe1() {
    pop_register16(registers.hl);
}

void CPU::load_memory_high_ram_c_a_0xe2() {
    memory.write_8(HIGH_RAM_START + registers.c, registers.a);
    registers.program_counter += 1;
    cycles_elapsed += 8;
}

void CPU::push_hl_0xe5() {
    push_register16(registers.hl);
}

void CPU::and_a_immediate8_0xe6() {
    registers.a &= memory.read_8(registers.program_counter + 1);
    set_flags(registers.a == 0, false, true, false);
    registers.program_counter += 2;
    cycles_elapsed += 8;
}

void CPU::restart_0x20_0xe7() {
    restart_address(0x20);
}

void CPU::add_sp_signed_immediate8_0xe8() {
    // Carries are based on the unsigned immediate byte while the result is based on its signed equivalent
    const auto unsigned_offset = memory.read_8(registers.program_counter + 1);
    const bool does_half_carry_occur = (registers.stack_pointer & 0x0f) + (unsigned_offset & 0x0f) > 0x0f;
    const bool does_carry_occur = (registers.stack_pointer & 0xff) + (unsigned_offset & 0xff) > 0xff;
    registers.stack_pointer += static_cast<int8_t>(unsigned_offset);
    set_flags(false, false, does_half_carry_occur, does_carry_occur);
    registers.program_counter += 2;
    cycles_elapsed += 16;
}

void CPU::jump_hl_0xe9() {
    registers.program_counter = registers.hl;
    cycles_elapsed += 4;
}

void CPU::load_memory_immediate16_a_0xea() {
    const auto destination_address = memory.read_16(registers.program_counter + 1);
    memory.write_8(destination_address, registers.a);
    registers.program_counter += 3;
    cycles_elapsed += 16;
}

// 0xeb is an unused opcode

// 0xec is an unused opcode

// 0xed is an unused opcode

void CPU::xor_a_immediate8_0xee() {
    registers.a ^= memory.read_8(registers.program_counter + 1);
    set_flags(registers.a == 0, false, false, false);
    registers.program_counter += 2;
    cycles_elapsed += 8;
}

void CPU::restart_0x28_0xef() {
    restart_address(0x28);
}

void CPU::load_a_memory_high_ram_immediate8_0xf0() {
    const auto unsigned_offset = memory.read_8(registers.program_counter + 1);
    registers.a = memory.read_8(HIGH_RAM_START + unsigned_offset);
    registers.program_counter += 2;
    cycles_elapsed += 12;
}

void CPU::pop_af_0xf1() {
    pop_register16(registers.af);
}

void CPU::load_a_memory_high_ram_c_0xf2() {
    registers.a = memory.read_8(HIGH_RAM_START + registers.c);
    registers.program_counter += 1;
    cycles_elapsed += 8;
}

void CPU::disable_interrupts_0xf3() {
    are_interrupts_enabled = false;
    registers.program_counter += 1;
    cycles_elapsed += 4;
}

// 0xf4 is an unused opcode

void CPU::push_af_0xf5() {
    push_register16(registers.af);
}

void CPU::or_a_immediate8_0xf6() {
    registers.a |= memory.read_8(registers.program_counter + 1);
    set_flags(registers.a == 0, false, false, false);
    registers.program_counter += 2;
    cycles_elapsed += 8;
}

void CPU::restart_0x30_0xf7() {
    restart_address(0x30);
}

void CPU::load_hl_stack_pointer_with_signed_offset_0xf8() {
    // Carries are based on the unsigned immediate byte while the result is based on its signed equivalent
    const auto unsigned_offset = memory.read_8(registers.program_counter + 1);
    const bool does_half_carry_occur = (registers.stack_pointer & 0x0f) + (unsigned_offset & 0x0f) > 0x0f;
    const bool does_carry_occur = (registers.stack_pointer & 0xff) + (unsigned_offset & 0xff) > 0xff;
    registers.hl = registers.stack_pointer + static_cast<int8_t>(unsigned_offset);
    set_flags(false, false, does_half_carry_occur, does_carry_occur);
    registers.program_counter += 2;
    cycles_elapsed += 12;
}

void CPU::load_stack_pointer_hl_0xf9() {
    registers.stack_pointer = registers.hl;
    registers.program_counter += 1;
    cycles_elapsed += 8;
}

void CPU::load_a_memory_immediate16_0xfa() {
    const auto source_address = memory.read_16(registers.program_counter + 1);
    registers.a = memory.read_8(source_address);
    registers.program_counter += 3;
    cycles_elapsed += 16;
}

void CPU::enable_interrupts_0xfb() {
    did_enable_interrupts_execute = true;
    registers.program_counter += 1;
    cycles_elapsed += 4;
}

// 0xfc is an unused opcode

// 0xfd is an unused opcode

void CPU::compare_a_immediate8_0xfe() {
    const auto immediate8 = memory.read_8(registers.program_counter + 1);
    const bool does_half_carry_occur = (registers.a & 0x0f) < (immediate8 & 0x0f);
    const bool does_carry_occur = registers.a < immediate8;
    set_flags(registers.a == immediate8, true, does_half_carry_occur, does_carry_occur);
    registers.program_counter += 2;
    cycles_elapsed += 8;
}

void CPU::restart_0x38_0xff() {
    restart_address(0x38);
}

} // namespace GameBoy
