#include <bit>
#include <iomanip>
#include <iostream>
#include "cpu.h"
#include "register_file.h"

namespace GameBoy {

void CPU::execute_next_instruction() {
    uint8_t next_instruction_opcode = memory.read_8(registers.program_counter);
    execute_instruction(next_instruction_opcode);
}

void CPU::reset_state() {
    RegisterFile<std::endian::native> initialized_register_file;
    update_registers(initialized_register_file);
    cycles_elapsed = 0;
    is_stopped = false;
    is_halted = false;
    are_interrupts_enabled = false;
    did_enable_interrupts_execute = false;
}

void CPU::set_post_boot_state() {
    // Updates flags based on header bytes 0x0134-0x014c in the loaded 'cartridge' ROM
    uint8_t header_checksum = 0;
    for (uint16_t address = 0x0134; address <= 0x014c; address++) {
        header_checksum -= memory.read_8(BOOTROM_SIZE + address) - 1;
    }

    registers.a = 0x01;
    update_flags(true, false, header_checksum != 0, header_checksum != 0);
    registers.b = 0x00;
    registers.c = 0x13;
    registers.d = 0x00;
    registers.e = 0xd8;
    registers.h = 0x01;
    registers.l = 0x4d;
    registers.program_counter = 0x100;
    registers.stack_pointer = 0xfffe;
}

RegisterFile<std::endian::native> CPU::get_register_file() const {
    return registers;
}

void CPU::update_registers(const RegisterFile<std::endian::native> &new_register_values) {
    registers.a = new_register_values.a;
    registers.flags = new_register_values.flags & 0xf0; // Lower nibble of flags must always be zeroed
    registers.bc = new_register_values.bc;
    registers.de = new_register_values.de;
    registers.hl = new_register_values.hl;
    registers.program_counter = new_register_values.program_counter;
    registers.stack_pointer = new_register_values.stack_pointer;
}

void CPU::print_register_values() const {
    std::cout << "=================== CPU Registers ===================\n";
    std::cout << std::hex << std::setfill('0');

    std::cout << "AF: 0x" << std::setw(4) << registers.af << "   "
              << "(A: 0x" << std::setw(2) << static_cast<int>(registers.a) << ","
              << " F: 0x" << std::setw(2) << static_cast<int>(registers.flags) << ")   "
              << "Flags ZNHC: " << ((registers.flags & FLAG_ZERO_MASK) ? "1" : "0")
                                << ((registers.flags & FLAG_SUBTRACT_MASK) ? "1" : "0")
                                << ((registers.flags & FLAG_HALF_CARRY_MASK) ? "1" : "0")
                                << ((registers.flags & FLAG_CARRY_MASK) ? "1" : "0") << "\n";

    std::cout << "BC: 0x" << std::setw(4) << registers.bc << "   "
              << "(B: 0x" << std::setw(2) << static_cast<int>(registers.b) << ","
              << " C: 0x" << std::setw(2) << static_cast<int>(registers.c) << ")\n";

    std::cout << "DE: 0x" << std::setw(4) << registers.de << "   "
              << "(D: 0x" << std::setw(2) << static_cast<int>(registers.d) << ","
              << " E: 0x" << std::setw(2) << static_cast<int>(registers.e) << ")\n";

    std::cout << "HL: 0x" << std::setw(4) << registers.hl << "   "
              << "(H: 0x" << std::setw(2) << static_cast<int>(registers.h) << ","
              << " L: 0x" << std::setw(2) << static_cast<int>(registers.l) << ")\n";

    std::cout << "Stack Pointer: 0x" << std::setw(4) << registers.stack_pointer << "\n";
    std::cout << "Program Counter: 0x" << std::setw(4) << registers.program_counter << "\n";

    std::cout << std::dec;
    std::cout << "Cycles Elapsed: " << cycles_elapsed << "\n";
    std::cout << "=====================================================\n";
}

uint16_t CPU::get_program_counter() const {
    return registers.program_counter;
}

const CPU::InstructionPointer CPU::instruction_table[0x100] = {
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
    &CPU::stop_0x10,
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
    &CPU::jump_relative_if_not_zero_signed_immediate8_0x20,
    &CPU::load_hl_immediate16_0x21,
    &CPU::load_memory_post_increment_hl_a_0x22,
    &CPU::increment_hl_0x23,
    &CPU::increment_h_0x24,
    &CPU::decrement_h_0x25,
    &CPU::load_h_immediate8_0x26,
    &CPU::decimal_adjust_a_0x27,
    &CPU::jump_relative_if_zero_signed_immediate8_0x28,
    &CPU::add_hl_hl_0x29,
    &CPU::load_a_memory_post_increment_hl_0x2a,
    &CPU::decrement_hl_0x2b,
    &CPU::increment_l_0x2c,
    &CPU::decrement_l_0x2d,
    &CPU::load_l_immediate8_0x2e,
    &CPU::complement_a_0x2f,
    &CPU::jump_relative_if_not_carry_signed_immediate8_0x30,
    &CPU::load_stack_pointer_immediate16_0x31,
    &CPU::load_memory_post_decrement_hl_a_0x32,
    &CPU::increment_stack_pointer_0x33,
    &CPU::increment_memory_hl_0x34,
    &CPU::decrement_memory_hl_0x35,
    &CPU::load_memory_hl_immediate8_0x36,
    &CPU::set_carry_flag_0x37,
    &CPU::jump_relative_if_carry_signed_immediate8_0x38,
    &CPU::add_hl_stack_pointer_0x39,
    &CPU::load_a_memory_post_decrement_hl_0x3a,
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
    &CPU::return_if_not_zero_0xc0,
    &CPU::pop_stack_bc_0xc1,
    &CPU::jump_if_not_zero_immediate16_0xc2,
    &CPU::jump_immediate16_0xc3,
    &CPU::call_if_not_zero_immediate16_0xc4,
    &CPU::push_stack_bc_0xc5,
    &CPU::add_a_immediate8_0xc6,
    &CPU::restart_at_0x00_0xc7,
    &CPU::return_if_zero_0xc8,
    &CPU::return_0xc9,
    &CPU::jump_if_zero_immediate16_0xca,
    &CPU::unused_opcode, // 0xcb is only used to prefix an extended instruction
    &CPU::call_if_zero_immediate16_0xcc,
    &CPU::call_immediate16_0xcd,
    &CPU::add_with_carry_a_immediate8_0xce,
    &CPU::restart_at_0x08_0xcf,
    &CPU::return_if_not_carry_0xd0,
    &CPU::pop_stack_de_0xd1,
    &CPU::jump_if_not_carry_immediate16_0xd2,
    &CPU::unused_opcode, // 0xd3 is an unused opcode
    &CPU::call_if_not_carry_immediate16_0xd4,
    &CPU::push_stack_de_0xd5,
    &CPU::subtract_a_immediate8_0xd6,
    &CPU::restart_at_0x10_0xd7,
    &CPU::return_if_carry_0xd8,
    &CPU::return_from_interrupt_0xd9,
    &CPU::jump_if_carry_immediate16_0xda,
    &CPU::unused_opcode, // 0xdb is an unused opcode
    &CPU::call_if_carry_immediate16_0xdc,
    &CPU::unused_opcode, // 0xdd is an unused opcode
    &CPU::subtract_with_carry_a_immediate8_0xde,
    &CPU::restart_at_0x18_0xdf,
    &CPU::load_memory_high_ram_offset_immediate8_a_0xe0,
    &CPU::pop_stack_hl_0xe1,
    &CPU::load_memory_high_ram_c_a_0xe2,
    &CPU::unused_opcode,
    &CPU::unused_opcode,
    &CPU::push_stack_hl_0xe5,
    &CPU::and_a_immediate8_0xe6,
    &CPU::restart_at_0x20_0xe7,
    &CPU::add_sp_signed_immediate8_0xe8,
    &CPU::jump_hl_0xe9,
    &CPU::load_memory_immediate16_a_0xea,
    &CPU::unused_opcode, // 0xeb is an unused opcode
    &CPU::unused_opcode, // 0xec is an unused opcode
    &CPU::unused_opcode, // 0xed is an unused opcode
    &CPU::xor_a_immediate8_0xee,
    &CPU::restart_at_0x28_0xef,
    &CPU::load_a_memory_high_ram_immediate8_0xf0,
    &CPU::pop_stack_af_0xf1,
    &CPU::load_a_memory_high_ram_c_0xf2,
    &CPU::disable_interrupts_0xf3,
    &CPU::unused_opcode, // 0xf4 is an unused opcode
    &CPU::push_stack_af_0xf5,
    &CPU::or_a_immediate8_0xf6,
    &CPU::restart_at_0x30_0xf7,
    &CPU::load_hl_stack_pointer_with_signed_offset_0xf8,
    &CPU::load_stack_pointer_hl_0xf9,
    &CPU::load_a_memory_immediate16_0xfa,
    &CPU::enable_interrupts_0xfb,
    &CPU::unused_opcode, // 0xfc is an unused opcode
    &CPU::unused_opcode, // 0xfd is an unused opcode
    &CPU::compare_a_immediate8_0xfe,
    &CPU::restart_at_0x38_0xff
};

const CPU::InstructionPointer CPU::extended_instruction_table[0x100] = {
    &CPU::rotate_left_circular_b_prefix_0x00,
    &CPU::rotate_left_circular_c_prefix_0x01,
    &CPU::rotate_left_circular_d_prefix_0x02,
    &CPU::rotate_left_circular_e_prefix_0x03,
    &CPU::rotate_left_circular_h_prefix_0x04,
    &CPU::rotate_left_circular_l_prefix_0x05,
    &CPU::rotate_left_circular_memory_hl_prefix_0x06,
    &CPU::rotate_left_circular_a_prefix_0x07,
    &CPU::rotate_right_circular_b_prefix_0x08,
    &CPU::rotate_right_circular_c_prefix_0x09,
    &CPU::rotate_right_circular_d_prefix_0x0a,
    &CPU::rotate_right_circular_e_prefix_0x0b,
    &CPU::rotate_right_circular_h_prefix_0x0c,
    &CPU::rotate_right_circular_l_prefix_0x0d,
    &CPU::rotate_right_circular_memory_hl_prefix_0x0e,
    &CPU::rotate_right_circular_a_prefix_0x0f,
    &CPU::rotate_left_through_carry_b_prefix_0x10,
    &CPU::rotate_left_through_carry_c_prefix_0x11,
    &CPU::rotate_left_through_carry_d_prefix_0x12,
    &CPU::rotate_left_through_carry_e_prefix_0x13,
    &CPU::rotate_left_through_carry_h_prefix_0x14,
    &CPU::rotate_left_through_carry_l_prefix_0x15,
    &CPU::rotate_left_through_carry_memory_hl_prefix_0x16,
    &CPU::rotate_left_through_carry_a_prefix_0x17,
    &CPU::rotate_right_through_carry_b_prefix_0x18,
    &CPU::rotate_right_through_carry_c_prefix_0x19,
    &CPU::rotate_right_through_carry_d_prefix_0x1a,
    &CPU::rotate_right_through_carry_e_prefix_0x1b,
    &CPU::rotate_right_through_carry_h_prefix_0x1c,
    &CPU::rotate_right_through_carry_l_prefix_0x1d,
    &CPU::rotate_right_through_carry_memory_hl_prefix_0x1e,
    &CPU::rotate_right_through_carry_a_prefix_0x1f,
    &CPU::shift_left_arithmetic_b_prefix_0x20,
    &CPU::shift_left_arithmetic_c_prefix_0x21,
    &CPU::shift_left_arithmetic_d_prefix_0x22,
    &CPU::shift_left_arithmetic_e_prefix_0x23,
    &CPU::shift_left_arithmetic_h_prefix_0x24,
    &CPU::shift_left_arithmetic_l_prefix_0x25,
    &CPU::shift_left_arithmetic_memory_hl_prefix_0x26,
    &CPU::shift_left_arithmetic_a_prefix_0x27,
    &CPU::shift_right_arithmetic_b_prefix_0x28,
    &CPU::shift_right_arithmetic_c_prefix_0x29,
    &CPU::shift_right_arithmetic_d_prefix_0x2a,
    &CPU::shift_right_arithmetic_e_prefix_0x2b,
    &CPU::shift_right_arithmetic_h_prefix_0x2c,
    &CPU::shift_right_arithmetic_l_prefix_0x2d,
    &CPU::shift_right_arithmetic_memory_hl_prefix_0x2e,
    &CPU::shift_right_arithmetic_a_prefix_0x2f,
    &CPU::swap_nibbles_b_prefix_0x30,
    &CPU::swap_nibbles_c_prefix_0x31,
    &CPU::swap_nibbles_d_prefix_0x32,
    &CPU::swap_nibbles_e_prefix_0x33,
    &CPU::swap_nibbles_h_prefix_0x34,
    &CPU::swap_nibbles_l_prefix_0x35,
    &CPU::swap_nibbles_memory_hl_prefix_0x36,
    &CPU::swap_nibbles_a_prefix_0x37,
    &CPU::shift_right_logical_b_prefix_0x38,
    &CPU::shift_right_logical_c_prefix_0x39,
    &CPU::shift_right_logical_d_prefix_0x3a,
    &CPU::shift_right_logical_e_prefix_0x3b,
    &CPU::shift_right_logical_h_prefix_0x3c,
    &CPU::shift_right_logical_l_prefix_0x3d,
    &CPU::shift_right_logical_memory_hl_prefix_0x3e,
    &CPU::shift_right_logical_a_prefix_0x3f,
    &CPU::test_bit_0_b_prefix_0x40,
    &CPU::test_bit_0_c_prefix_0x41,
    &CPU::test_bit_0_d_prefix_0x42,
    &CPU::test_bit_0_e_prefix_0x43,
    &CPU::test_bit_0_h_prefix_0x44,
    &CPU::test_bit_0_l_prefix_0x45,
    &CPU::test_bit_0_memory_hl_prefix_0x46,
    &CPU::test_bit_0_a_prefix_0x47,
    &CPU::test_bit_1_b_prefix_0x48,
    &CPU::test_bit_1_c_prefix_0x49,
    &CPU::test_bit_1_d_prefix_0x4a,
    &CPU::test_bit_1_e_prefix_0x4b,
    &CPU::test_bit_1_h_prefix_0x4c,
    &CPU::test_bit_1_l_prefix_0x4d,
    &CPU::test_bit_1_memory_hl_prefix_0x4e,
    &CPU::test_bit_1_a_prefix_0x4f,
    &CPU::test_bit_2_b_prefix_0x50,
    &CPU::test_bit_2_c_prefix_0x51,
    &CPU::test_bit_2_d_prefix_0x52,
    &CPU::test_bit_2_e_prefix_0x53,
    &CPU::test_bit_2_h_prefix_0x54,
    &CPU::test_bit_2_l_prefix_0x55,
    &CPU::test_bit_2_memory_hl_prefix_0x56,
    &CPU::test_bit_2_a_prefix_0x57,
    &CPU::test_bit_3_b_prefix_0x58,
    &CPU::test_bit_3_c_prefix_0x59,
    &CPU::test_bit_3_d_prefix_0x5a,
    &CPU::test_bit_3_e_prefix_0x5b,
    &CPU::test_bit_3_h_prefix_0x5c,
    &CPU::test_bit_3_l_prefix_0x5d,
    &CPU::test_bit_3_memory_hl_prefix_0x5e,
    &CPU::test_bit_3_a_prefix_0x5f,
    &CPU::test_bit_4_b_prefix_0x60,
    &CPU::test_bit_4_c_prefix_0x61,
    &CPU::test_bit_4_d_prefix_0x62,
    &CPU::test_bit_4_e_prefix_0x63,
    &CPU::test_bit_4_h_prefix_0x64,
    &CPU::test_bit_4_l_prefix_0x65,
    &CPU::test_bit_4_memory_hl_prefix_0x66,
    &CPU::test_bit_4_a_prefix_0x67,
    &CPU::test_bit_5_b_prefix_0x68,
    &CPU::test_bit_5_c_prefix_0x69,
    &CPU::test_bit_5_d_prefix_0x6a,
    &CPU::test_bit_5_e_prefix_0x6b,
    &CPU::test_bit_5_h_prefix_0x6c,
    &CPU::test_bit_5_l_prefix_0x6d,
    &CPU::test_bit_5_memory_hl_prefix_0x6e,
    &CPU::test_bit_5_a_prefix_0x6f,
    &CPU::test_bit_6_b_prefix_0x70,
    &CPU::test_bit_6_c_prefix_0x71,
    &CPU::test_bit_6_d_prefix_0x72,
    &CPU::test_bit_6_e_prefix_0x73,
    &CPU::test_bit_6_h_prefix_0x74,
    &CPU::test_bit_6_l_prefix_0x75,
    &CPU::test_bit_6_memory_hl_prefix_0x76,
    &CPU::test_bit_6_a_prefix_0x77,
    &CPU::test_bit_7_b_prefix_0x78,
    &CPU::test_bit_7_c_prefix_0x79,
    &CPU::test_bit_7_d_prefix_0x7a,
    &CPU::test_bit_7_e_prefix_0x7b,
    &CPU::test_bit_7_h_prefix_0x7c,
    &CPU::test_bit_7_l_prefix_0x7d,
    &CPU::test_bit_7_memory_hl_prefix_0x7e,
    &CPU::test_bit_7_a_prefix_0x7f,
    &CPU::reset_bit_0_b_prefix_0x80,
    &CPU::reset_bit_0_c_prefix_0x81,
    &CPU::reset_bit_0_d_prefix_0x82,
    &CPU::reset_bit_0_e_prefix_0x83,
    &CPU::reset_bit_0_h_prefix_0x84,
    &CPU::reset_bit_0_l_prefix_0x85,
    &CPU::reset_bit_0_memory_hl_prefix_0x86,
    &CPU::reset_bit_0_a_prefix_0x87,
    &CPU::reset_bit_1_b_prefix_0x88,
    &CPU::reset_bit_1_c_prefix_0x89,
    &CPU::reset_bit_1_d_prefix_0x8a,
    &CPU::reset_bit_1_e_prefix_0x8b,
    &CPU::reset_bit_1_h_prefix_0x8c,
    &CPU::reset_bit_1_l_prefix_0x8d,
    &CPU::reset_bit_1_memory_hl_prefix_0x8e,
    &CPU::reset_bit_1_a_prefix_0x8f,
    &CPU::reset_bit_2_b_prefix_0x90,
    &CPU::reset_bit_2_c_prefix_0x91,
    &CPU::reset_bit_2_d_prefix_0x92,
    &CPU::reset_bit_2_e_prefix_0x93,
    &CPU::reset_bit_2_h_prefix_0x94,
    &CPU::reset_bit_2_l_prefix_0x95,
    &CPU::reset_bit_2_memory_hl_prefix_0x96,
    &CPU::reset_bit_2_a_prefix_0x97,
    &CPU::reset_bit_3_b_prefix_0x98,
    &CPU::reset_bit_3_c_prefix_0x99,
    &CPU::reset_bit_3_d_prefix_0x9a,
    &CPU::reset_bit_3_e_prefix_0x9b,
    &CPU::reset_bit_3_h_prefix_0x9c,
    &CPU::reset_bit_3_l_prefix_0x9d,
    &CPU::reset_bit_3_memory_hl_prefix_0x9e,
    &CPU::reset_bit_3_a_prefix_0x9f,
    &CPU::reset_bit_4_b_prefix_0xa0,
    &CPU::reset_bit_4_c_prefix_0xa1,
    &CPU::reset_bit_4_d_prefix_0xa2,
    &CPU::reset_bit_4_e_prefix_0xa3,
    &CPU::reset_bit_4_h_prefix_0xa4,
    &CPU::reset_bit_4_l_prefix_0xa5,
    &CPU::reset_bit_4_memory_hl_prefix_0xa6,
    &CPU::reset_bit_4_a_prefix_0xa7,
    &CPU::reset_bit_5_b_prefix_0xa8,
    &CPU::reset_bit_5_c_prefix_0xa9,
    &CPU::reset_bit_5_d_prefix_0xaa,
    &CPU::reset_bit_5_e_prefix_0xab,
    &CPU::reset_bit_5_h_prefix_0xac,
    &CPU::reset_bit_5_l_prefix_0xad,
    &CPU::reset_bit_5_memory_hl_prefix_0xae,
    &CPU::reset_bit_5_a_prefix_0xaf,
    &CPU::reset_bit_6_b_prefix_0xb0,
    &CPU::reset_bit_6_c_prefix_0xb1,
    &CPU::reset_bit_6_d_prefix_0xb2,
    &CPU::reset_bit_6_e_prefix_0xb3,
    &CPU::reset_bit_6_h_prefix_0xb4,
    &CPU::reset_bit_6_l_prefix_0xb5,
    &CPU::reset_bit_6_memory_hl_prefix_0xb6,
    &CPU::reset_bit_6_a_prefix_0xb7,
    &CPU::reset_bit_7_b_prefix_0xb8,
    &CPU::reset_bit_7_c_prefix_0xb9,
    &CPU::reset_bit_7_d_prefix_0xba,
    &CPU::reset_bit_7_e_prefix_0xbb,
    &CPU::reset_bit_7_h_prefix_0xbc,
    &CPU::reset_bit_7_l_prefix_0xbd,
    &CPU::reset_bit_7_memory_hl_prefix_0xbe,
    &CPU::reset_bit_7_a_prefix_0xbf,
    &CPU::set_bit_0_b_prefix_0xc0,
    &CPU::set_bit_0_c_prefix_0xc1,
    &CPU::set_bit_0_d_prefix_0xc2,
    &CPU::set_bit_0_e_prefix_0xc3,
    &CPU::set_bit_0_h_prefix_0xc4,
    &CPU::set_bit_0_l_prefix_0xc5,
    &CPU::set_bit_0_memory_hl_prefix_0xc6,
    &CPU::set_bit_0_a_prefix_0xc7,
    &CPU::set_bit_1_b_prefix_0xc8,
    &CPU::set_bit_1_c_prefix_0xc9,
    &CPU::set_bit_1_d_prefix_0xca,
    &CPU::set_bit_1_e_prefix_0xcb,
    &CPU::set_bit_1_h_prefix_0xcc,
    &CPU::set_bit_1_l_prefix_0xcd,
    &CPU::set_bit_1_memory_hl_prefix_0xce,
    &CPU::set_bit_1_a_prefix_0xcf,
    &CPU::set_bit_2_b_prefix_0xd0,
    &CPU::set_bit_2_c_prefix_0xd1,
    &CPU::set_bit_2_d_prefix_0xd2,
    &CPU::set_bit_2_e_prefix_0xd3,
    &CPU::set_bit_2_h_prefix_0xd4,
    &CPU::set_bit_2_l_prefix_0xd5,
    &CPU::set_bit_2_memory_hl_prefix_0xd6,
    &CPU::set_bit_2_a_prefix_0xd7,
    &CPU::set_bit_3_b_prefix_0xd8,
    &CPU::set_bit_3_c_prefix_0xd9,
    &CPU::set_bit_3_d_prefix_0xda,
    &CPU::set_bit_3_e_prefix_0xdb,
    &CPU::set_bit_3_h_prefix_0xdc,
    &CPU::set_bit_3_l_prefix_0xdd,
    &CPU::set_bit_3_memory_hl_prefix_0xde,
    &CPU::set_bit_3_a_prefix_0xdf,
    &CPU::set_bit_4_b_prefix_0xe0,
    &CPU::set_bit_4_c_prefix_0xe1,
    &CPU::set_bit_4_d_prefix_0xe2,
    &CPU::set_bit_4_e_prefix_0xe3,
    &CPU::set_bit_4_h_prefix_0xe4,
    &CPU::set_bit_4_l_prefix_0xe5,
    &CPU::set_bit_4_memory_hl_prefix_0xe6,
    &CPU::set_bit_4_a_prefix_0xe7,
    &CPU::set_bit_5_b_prefix_0xe8,
    &CPU::set_bit_5_c_prefix_0xe9,
    &CPU::set_bit_5_d_prefix_0xea,
    &CPU::set_bit_5_e_prefix_0xeb,
    &CPU::set_bit_5_h_prefix_0xec,
    &CPU::set_bit_5_l_prefix_0xed,
    &CPU::set_bit_5_memory_hl_prefix_0xee,
    &CPU::set_bit_5_a_prefix_0xef,
    &CPU::set_bit_6_b_prefix_0xf0,
    &CPU::set_bit_6_c_prefix_0xf1,
    &CPU::set_bit_6_d_prefix_0xf2,
    &CPU::set_bit_6_e_prefix_0xf3,
    &CPU::set_bit_6_h_prefix_0xf4,
    &CPU::set_bit_6_l_prefix_0xf5,
    &CPU::set_bit_6_memory_hl_prefix_0xf6,
    &CPU::set_bit_6_a_prefix_0xf7,
    &CPU::set_bit_7_b_prefix_0xf8,
    &CPU::set_bit_7_c_prefix_0xf9,
    &CPU::set_bit_7_d_prefix_0xfa,
    &CPU::set_bit_7_e_prefix_0xfb,
    &CPU::set_bit_7_h_prefix_0xfc,
    &CPU::set_bit_7_l_prefix_0xfd,
    &CPU::set_bit_7_memory_hl_prefix_0xfe,
    &CPU::set_bit_7_a_prefix_0xff
};

// ===============================
// ===== Instruction Helpers =====
// ===============================

void CPU::execute_instruction(uint8_t opcode) {
    if (opcode != INSTRUCTION_PREFIX_BYTE) {
        (this->*instruction_table[opcode])();
    }
    else {
        const uint8_t prefixed_opcode = memory.read_8(registers.program_counter + 1);
        (this->*extended_instruction_table[prefixed_opcode])();
    }

    if (did_enable_interrupts_execute && opcode != ENABLE_INTERRUPTS_OPCODE) {
        are_interrupts_enabled = true;
        did_enable_interrupts_execute = false;
    }
}

void CPU::update_flags(bool new_zero_value, bool new_subtract_value, bool new_half_carry_value, bool new_carry_value) {
    registers.flags = (new_zero_value ? FLAG_ZERO_MASK : 0) |
                      (new_subtract_value ? FLAG_SUBTRACT_MASK : 0) |
                      (new_half_carry_value ? FLAG_HALF_CARRY_MASK : 0) |
                      (new_carry_value ? FLAG_CARRY_MASK : 0);
}

bool CPU::is_flag_set(uint8_t flag_mask) const {
    return (registers.flags & flag_mask) != 0;
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
    update_flags(register8 == 0, false, does_half_carry_occur, is_flag_set(FLAG_CARRY_MASK));
    registers.program_counter += 1;
    cycles_elapsed += 4;
}

void CPU::decrement_register8(uint8_t &register8) {
    const bool does_half_carry_occur = (register8 & 0x0f) == 0x00;
    register8--;
    update_flags(register8 == 0, true, does_half_carry_occur, is_flag_set(FLAG_CARRY_MASK));
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
    update_flags(is_flag_set(FLAG_ZERO_MASK), false, does_half_carry_occur, does_carry_occur);
    registers.program_counter += 1;
    cycles_elapsed += 8;
}

void CPU::add_a_register8(const uint8_t &register8) {
    const bool does_half_carry_occur = (registers.a & 0x0f) + (register8 & 0x0f) > 0x0f;
    const bool does_carry_occur = static_cast<uint16_t>(registers.a) + register8 > 0xff;
    registers.a += register8;
    update_flags(registers.a == 0, false, does_half_carry_occur, does_carry_occur);
    registers.program_counter += 1;
    cycles_elapsed += 4;
}

void CPU::add_with_carry_a_register8(const uint8_t &register8) {
    const uint8_t carry_in = is_flag_set(FLAG_CARRY_MASK) ? 1 : 0;
    const bool does_half_carry_occur = (registers.a & 0x0f) + (register8 & 0x0f) + carry_in > 0x0f;
    const bool does_carry_occur = static_cast<uint16_t>(registers.a) + register8 + carry_in > 0xff;
    registers.a += register8 + carry_in;
    update_flags(registers.a == 0, false, does_half_carry_occur, does_carry_occur);
    registers.program_counter += 1;
    cycles_elapsed += 4;
}

void CPU::subtract_a_register8(const uint8_t &register8) {
    const bool does_half_carry_occur = (registers.a & 0x0f) < (register8 & 0x0f);
    const bool does_carry_occur = registers.a < register8;
    registers.a -= register8;
    update_flags(registers.a == 0, true, does_half_carry_occur, does_carry_occur);
    registers.program_counter += 1;
    cycles_elapsed += 4;
}

void CPU::subtract_with_carry_a_register8(const uint8_t &register8) {
    const uint8_t carry_in = is_flag_set(FLAG_CARRY_MASK) ? 1 : 0;
    const bool does_half_carry_occur = (registers.a & 0x0f) < (register8 & 0x0f) + carry_in;
    const bool does_carry_occur = registers.a < register8 + carry_in;
    registers.a -= register8 + carry_in;
    update_flags(registers.a == 0, true, does_half_carry_occur, does_carry_occur);
    registers.program_counter += 1;
    cycles_elapsed += 4;
}

void CPU::and_a_register8(const uint8_t &register8) {
    registers.a &= register8;
    update_flags(registers.a == 0, false, true, false);
    registers.program_counter += 1;
    cycles_elapsed += 4;
}

void CPU::xor_a_register8(const uint8_t &register8) {
    registers.a ^= register8;
    update_flags(registers.a == 0, false, false, false);
    registers.program_counter += 1;
    cycles_elapsed += 4;
}

void CPU::or_a_register8(const uint8_t &register8) {
    registers.a |= register8;
    update_flags(registers.a == 0, false, false, false);
    registers.program_counter += 1;
    cycles_elapsed += 4;
}

void CPU::compare_a_register8(const uint8_t &register8) {
    const bool does_half_carry_occur = (registers.a & 0x0f) < (register8 & 0x0f);
    const bool does_carry_occur = registers.a < register8;
    update_flags(registers.a == register8, true, does_half_carry_occur, does_carry_occur);
    registers.program_counter += 1;
    cycles_elapsed += 4;
}

void CPU::jump_relative_conditional_signed_immediate8(bool is_condition_met) {
    if (is_condition_met) {
        const int8_t signed_offset = static_cast<int8_t>(memory.read_8(registers.program_counter + 1));
        registers.program_counter += 2 + signed_offset;
        cycles_elapsed += 12;
    } 
    else {
        registers.program_counter += 2;
        cycles_elapsed += 8;
    }
}

void CPU::jump_conditional_immediate16(bool is_condition_met) {
    if (is_condition_met) {
        registers.program_counter = memory.read_16(registers.program_counter + 1);
        cycles_elapsed += 16;
    } 
    else {
        registers.program_counter += 3;
        cycles_elapsed += 12;
    }
}

void CPU::call_conditional_immediate16(bool is_condition_met) {
    if (is_condition_met) {
        const uint16_t return_address = registers.program_counter + 3;
        registers.stack_pointer -= 2;
        memory.write_16(registers.stack_pointer, return_address);
        registers.program_counter = memory.read_16(registers.program_counter + 1);
        cycles_elapsed += 24;
    } 
    else {
        registers.program_counter += 3;
        cycles_elapsed += 12;
    }
}

void CPU::return_conditional(bool is_condition_met) {
    if (is_condition_met) {
        registers.program_counter = memory.read_16(registers.stack_pointer);
        registers.stack_pointer += 2;
        cycles_elapsed += 20;
    } 
    else {
        registers.program_counter += 1;
        cycles_elapsed += 8;
    }
}

void CPU::push_stack_register16(const uint16_t &register16) {
    registers.stack_pointer -= 2;
    memory.write_16(registers.stack_pointer, register16);
    registers.program_counter += 1;
    cycles_elapsed += 16;
}

void CPU::pop_stack_register16(uint16_t &register16) {
    register16 = memory.read_16(registers.stack_pointer);
    registers.stack_pointer += 2;
    registers.program_counter += 1;
    cycles_elapsed += 12;
}

void CPU::restart_at_address(uint16_t address) {
    const uint16_t return_address = registers.program_counter + 1;
    registers.stack_pointer -= 2;
    memory.write_16(registers.stack_pointer, return_address);
    registers.program_counter = address;
    cycles_elapsed += 16;
}

void CPU::rotate_left_circular_register8(uint8_t &register8) {
    const bool does_carry_occur = (register8 & 0b10000000) != 0;
    register8 = (register8 << 1) | (register8 >> 7);
    update_flags(register8 == 0, false, false, does_carry_occur);
    registers.program_counter += 2;
    cycles_elapsed += 8;
}

void CPU::rotate_right_circular_register8(uint8_t &register8) {
    const bool does_carry_occur = (register8 & 0b00000001) != 0;
    register8 = (register8 << 7) | (register8 >> 1);
    update_flags(register8 == 0, false, false, does_carry_occur);
    registers.program_counter += 2;
    cycles_elapsed += 8;
}

void CPU::rotate_left_through_carry_register8(uint8_t &register8) {
    const uint8_t carry_in = is_flag_set(FLAG_CARRY_MASK) ? 1 : 0;
    const bool does_carry_occur = (register8 & 0b10000000) != 0;
    register8 = (register8 << 1) | carry_in;
    update_flags(register8 == 0, false, false, does_carry_occur);
    registers.program_counter += 2;
    cycles_elapsed += 8;
}

void CPU::rotate_right_through_carry_register8(uint8_t &register8) {
    const uint8_t carry_in = is_flag_set(FLAG_CARRY_MASK) ? 1 : 0;
    const bool does_carry_occur = (register8 & 0b00000001) != 0;
    register8 = (carry_in << 7) | (register8 >> 1);
    update_flags(register8 == 0, false, false, does_carry_occur);
    registers.program_counter += 2;
    cycles_elapsed += 8;
}

void CPU::shift_left_arithmetic_register8(uint8_t &register8) {
    const bool does_carry_occur = (register8 & 0b10000000) != 0;
    register8 <<= 1;
    update_flags(register8 == 0, false, false, does_carry_occur);
    registers.program_counter += 2;
    cycles_elapsed += 8;
}

void CPU::shift_right_arithmetic_register8(uint8_t &register8) {
    const bool does_carry_occur = (register8 & 0b00000001) != 0;
    const uint8_t preserved_sign_bit = register8 & 0b10000000;
    register8 = preserved_sign_bit | (register8 >> 1);
    update_flags(register8 == 0, false, false, does_carry_occur);
    registers.program_counter += 2;
    cycles_elapsed += 8;
}

void CPU::swap_nibbles_register8(uint8_t &register8) {
    register8 = ((register8 & 0x0f) << 4) | ((register8 & 0xf0) >> 4);
    update_flags(register8 == 0, false, false, false);
    registers.program_counter += 2;
    cycles_elapsed += 8;
}

void CPU::shift_right_logical_register8(uint8_t &register8) {
    const bool does_carry_occur = (register8 & 0b00000001) != 0;
    register8 >>= 1;
    update_flags(register8 == 0, false, false, does_carry_occur);
    registers.program_counter += 2;
    cycles_elapsed += 8;
}

void CPU::test_bit_position_register8(uint8_t bit_position_to_test, const uint8_t &register8) {
    const bool is_bit_set = (register8 & (1 << bit_position_to_test)) != 0;
    update_flags(!is_bit_set, false, true, is_flag_set(FLAG_CARRY_MASK));
    registers.program_counter += 2;
    cycles_elapsed += 8;
}

void CPU::test_bit_position_memory_hl(uint8_t bit_position_to_test) {
    uint8_t memory_hl = memory.read_8(registers.hl);
    const bool is_bit_set = (memory_hl & (1 << bit_position_to_test)) != 0;
    update_flags(!is_bit_set, false, true, is_flag_set(FLAG_CARRY_MASK));
    registers.program_counter += 2;
    cycles_elapsed += 12;
}

void CPU::reset_bit_position_register8(uint8_t bit_position_to_reset, uint8_t &register8) {
    register8 &= ~(1 << bit_position_to_reset);
    registers.program_counter += 2;
    cycles_elapsed += 8;
}

void CPU::reset_bit_position_memory_hl(uint8_t bit_position_to_reset) {
    uint8_t memory_hl = memory.read_8(registers.hl);
    memory_hl &= ~(1 << bit_position_to_reset);
    memory.write_8(registers.hl, memory_hl);
    registers.program_counter += 2;
    cycles_elapsed += 16;
}

void CPU::set_bit_position_register8(uint8_t bit_position_to_set, uint8_t &register8) {
    register8 |= (1 << bit_position_to_set);
    registers.program_counter += 2;
    cycles_elapsed += 8;
}

void CPU::set_bit_position_memory_hl(uint8_t bit_position_to_set) {
    uint8_t memory_hl = memory.read_8(registers.hl);
    memory_hl |= (1 << bit_position_to_set);
    memory.write_8(registers.hl, memory_hl);
    registers.program_counter += 2;
    cycles_elapsed += 16;
}

// ========================
// ===== Instructions =====
// ========================

void CPU::unused_opcode() {
    std::cerr << std::hex << std::setfill('0');
    std::cerr << "Warning: Unused opcode 0x" << std::setw(2) 
              << static_cast<int>(memory.read_8(registers.program_counter)) << " "
              << "encountered at memory address 0x" << std::setw(2) 
              << static_cast<int>(registers.program_counter) << "\n";
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
    const bool does_carry_occur = (registers.a & 0b10000000) != 0;
    registers.a = (registers.a << 1) | (registers.a >> 7);
    update_flags(false, false, false, does_carry_occur);
    registers.program_counter += 1;
    cycles_elapsed += 4;
}

void CPU::load_memory_immediate16_stack_pointer_0x08() {
    const uint16_t immediate16 = memory.read_16(registers.program_counter + 1);
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
    const bool does_carry_occur = (registers.a & 0b00000001) != 0;
    registers.a = (registers.a << 7) | (registers.a >> 1);
    update_flags(false, false, false, does_carry_occur);
    registers.program_counter += 1;
    cycles_elapsed += 4;
}

void CPU::stop_0x10() {
    // TODO handle skipping the next byte in memory elsewhere
    is_stopped = true;
    registers.program_counter += 1;
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
    const uint8_t carry_in = is_flag_set(FLAG_CARRY_MASK) ? 1 : 0;
    const bool does_carry_occur = (registers.a & 0b10000000) != 0;
    registers.a = (registers.a << 1) | carry_in;
    update_flags(false, false, false, does_carry_occur);
    registers.program_counter += 1;
    cycles_elapsed += 4;
}

void CPU::jump_relative_signed_immediate8_0x18() {
    jump_relative_conditional_signed_immediate8(true);
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
    const uint8_t carry_in = is_flag_set(FLAG_CARRY_MASK) ? 1 : 0;
    const bool does_carry_occur = (registers.a & 0b00000001) != 0;
    registers.a = (carry_in << 7) | (registers.a >> 1);
    update_flags(false, false, false, does_carry_occur);
    registers.program_counter += 1;
    cycles_elapsed += 4;
}

void CPU::jump_relative_if_not_zero_signed_immediate8_0x20() {
    jump_relative_conditional_signed_immediate8(!is_flag_set(FLAG_ZERO_MASK));
}

void CPU::load_hl_immediate16_0x21() {
    load_register16_immediate16(registers.hl);
}

void CPU::load_memory_post_increment_hl_a_0x22() {
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
    const bool was_addition_most_recent = !is_flag_set(FLAG_SUBTRACT_MASK);
    bool does_carry_occur = false;
    uint8_t adjustment = 0;
    // Previous operation was between two binary coded decimals (BCDs) and this corrects register A back to BCD format
    if (is_flag_set(FLAG_HALF_CARRY_MASK) || (was_addition_most_recent && (registers.a & 0x0f) > 0x09)) {
        adjustment |= 0x06;
    }
    if (is_flag_set(FLAG_CARRY_MASK) || (was_addition_most_recent && registers.a > 0x99)) {
        adjustment |= 0x60;
        does_carry_occur = true;
    }
    registers.a = was_addition_most_recent ? (registers.a + adjustment) : (registers.a - adjustment);
    update_flags(registers.a == 0, is_flag_set(FLAG_SUBTRACT_MASK), false, does_carry_occur);
    registers.program_counter += 1;
    cycles_elapsed += 4;
}

void CPU::jump_relative_if_zero_signed_immediate8_0x28() {
    jump_relative_conditional_signed_immediate8(is_flag_set(FLAG_ZERO_MASK));
}

void CPU::add_hl_hl_0x29() {
    add_hl_register16(registers.hl);
}

void CPU::load_a_memory_post_increment_hl_0x2a() {
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
    update_flags(is_flag_set(FLAG_ZERO_MASK), true, true, is_flag_set(FLAG_CARRY_MASK));
    registers.program_counter += 1;
    cycles_elapsed += 4;
}

void CPU::jump_relative_if_not_carry_signed_immediate8_0x30() {
    jump_relative_conditional_signed_immediate8(!is_flag_set(FLAG_CARRY_MASK));
}

void CPU::load_stack_pointer_immediate16_0x31() {
    load_register16_immediate16(registers.stack_pointer);
}

void CPU::load_memory_post_decrement_hl_a_0x32() {
    load_memory_register16_register8(registers.hl--, registers.a);
}

void CPU::increment_stack_pointer_0x33() {
    increment_register16(registers.stack_pointer);
}

void CPU::increment_memory_hl_0x34() {
    uint8_t memory_hl = memory.read_8(registers.hl);
    const bool does_half_carry_occur = (memory_hl & 0x0f) == 0x0f;
    memory.write_8(registers.hl, ++memory_hl);
    update_flags(memory_hl == 0, false, does_half_carry_occur, is_flag_set(FLAG_CARRY_MASK));
    registers.program_counter += 1;
    cycles_elapsed += 12;
}

void CPU::decrement_memory_hl_0x35() {
    uint8_t memory_hl = memory.read_8(registers.hl);
    const bool does_half_carry_occur = (memory_hl & 0x0f) == 0x00;
    memory.write_8(registers.hl, --memory_hl);
    update_flags(memory_hl == 0, true, does_half_carry_occur, is_flag_set(FLAG_CARRY_MASK));
    registers.program_counter += 1;
    cycles_elapsed += 12;
}

void CPU::load_memory_hl_immediate8_0x36() {
    memory.write_8(registers.hl, memory.read_8(registers.program_counter + 1));
    registers.program_counter += 2;
    cycles_elapsed += 12;
}

void CPU::set_carry_flag_0x37() {
    update_flags(is_flag_set(FLAG_ZERO_MASK), false, false, true);
    registers.program_counter += 1;
    cycles_elapsed += 4;
}

void CPU::jump_relative_if_carry_signed_immediate8_0x38() {
    jump_relative_conditional_signed_immediate8(is_flag_set(FLAG_CARRY_MASK));
}

void CPU::add_hl_stack_pointer_0x39() {
    add_hl_register16(registers.stack_pointer);
}

void CPU::load_a_memory_post_decrement_hl_0x3a() {
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
    update_flags(is_flag_set(FLAG_ZERO_MASK), false, false, !is_flag_set(FLAG_CARRY_MASK));
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
    const uint8_t memory_hl = memory.read_8(registers.hl);
    const bool does_half_carry_occur = (registers.a & 0x0f) + (memory_hl & 0x0f) > 0x0f;
    const bool does_carry_occur = static_cast<uint16_t>(registers.a) + memory_hl > 0xff;
    registers.a += memory_hl;
    update_flags(registers.a == 0, false, does_half_carry_occur, does_carry_occur);
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
    const uint8_t memory_hl = memory.read_8(registers.hl);
    const uint8_t carry_in = is_flag_set(FLAG_CARRY_MASK) ? 1 : 0;
    const bool does_half_carry_occur = (registers.a & 0x0f) + (memory_hl & 0x0f) + carry_in > 0x0f;
    const bool does_carry_occur = static_cast<uint16_t>(registers.a) + memory_hl + carry_in > 0xff;
    registers.a += memory_hl + carry_in;
    update_flags(registers.a == 0, false, does_half_carry_occur, does_carry_occur);
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
    const uint8_t memory_hl = memory.read_8(registers.hl);
    const bool does_half_carry_occur = (registers.a & 0x0f) < (memory_hl & 0x0f);
    const bool does_carry_occur = registers.a < memory_hl;
    registers.a -= memory_hl;
    update_flags(registers.a == 0, true, does_half_carry_occur, does_carry_occur);
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
    const uint8_t memory_hl = memory.read_8(registers.hl);
    const uint8_t carry_in = is_flag_set(FLAG_CARRY_MASK) ? 1 : 0;
    const bool does_half_carry_occur = (registers.a & 0x0f) < (memory_hl & 0x0f) + carry_in;
    const bool does_carry_occur = registers.a < memory_hl + carry_in;
    registers.a -= memory_hl + carry_in;
    update_flags(registers.a == 0, true, does_half_carry_occur, does_carry_occur);
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
    update_flags(registers.a == 0, false, true, false);
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
    update_flags(registers.a == 0, false, false, false);
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
    update_flags(registers.a == 0, false, false, false);
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
    const uint8_t memory_hl = memory.read_8(registers.hl);
    const bool does_half_carry_occur = (registers.a & 0x0f) < (memory_hl & 0x0f);
    const bool does_carry_occur = registers.a < memory_hl;
    update_flags(registers.a == memory_hl, true, does_half_carry_occur, does_carry_occur);
    registers.program_counter += 1;
    cycles_elapsed += 8;
}

void CPU::compare_a_a_0xbf() {
    compare_a_register8(registers.a);
}

void CPU::return_if_not_zero_0xc0() {
    return_conditional(!is_flag_set(FLAG_ZERO_MASK));
}

void CPU::pop_stack_bc_0xc1() {
    pop_stack_register16(registers.bc);
}

void CPU::jump_if_not_zero_immediate16_0xc2() {
    jump_conditional_immediate16(!is_flag_set(FLAG_ZERO_MASK));
}

void CPU::jump_immediate16_0xc3() {
    jump_conditional_immediate16(true);
}

void CPU::call_if_not_zero_immediate16_0xc4() {
    call_conditional_immediate16(!is_flag_set(FLAG_ZERO_MASK));
}

void CPU::push_stack_bc_0xc5() {
    push_stack_register16(registers.bc);
}

void CPU::add_a_immediate8_0xc6() {
    const uint8_t immediate8 = memory.read_8(registers.program_counter + 1);
    const bool does_half_carry_occur = (registers.a & 0x0f) + (immediate8 & 0x0f) > 0x0f;
    const bool does_carry_occur = static_cast<uint16_t>(registers.a) + immediate8 > 0xff;
    registers.a += immediate8;
    update_flags(registers.a == 0, false, does_half_carry_occur, does_carry_occur);
    registers.program_counter += 2;
    cycles_elapsed += 8;
}

void CPU::restart_at_0x00_0xc7() {
    restart_at_address(0x00);
}

void CPU::return_if_zero_0xc8() {
    return_conditional(is_flag_set(FLAG_ZERO_MASK));
}

void CPU::return_0xc9() {
    registers.program_counter = memory.read_16(registers.stack_pointer);
    registers.stack_pointer += 2;
    cycles_elapsed += 16;
}

void CPU::jump_if_zero_immediate16_0xca() {
    jump_conditional_immediate16(is_flag_set(FLAG_ZERO_MASK));
}

// 0xcb is only used to prefix an extended instruction

void CPU::call_if_zero_immediate16_0xcc() {
    call_conditional_immediate16(is_flag_set(FLAG_ZERO_MASK));
}

void CPU::call_immediate16_0xcd() {
    call_conditional_immediate16(true);
}

void CPU::add_with_carry_a_immediate8_0xce() {
    const uint8_t immediate8 = memory.read_8(registers.program_counter + 1);
    const uint8_t carry_in = is_flag_set(FLAG_CARRY_MASK) ? 1 : 0;
    const bool does_half_carry_occur = (registers.a & 0x0f) + (immediate8 & 0x0f) + carry_in > 0x0f;
    const bool does_carry_occur = static_cast<uint16_t>(registers.a) + immediate8 + carry_in > 0xff;
    registers.a += immediate8 + carry_in;
    update_flags(registers.a == 0, false, does_half_carry_occur, does_carry_occur);
    registers.program_counter += 2;
    cycles_elapsed += 8;
}

void CPU::restart_at_0x08_0xcf() {
    restart_at_address(0x08);
}

void CPU::return_if_not_carry_0xd0() {
    return_conditional(!is_flag_set(FLAG_CARRY_MASK));
}

void CPU::pop_stack_de_0xd1() {
    pop_stack_register16(registers.de);
}

void CPU::jump_if_not_carry_immediate16_0xd2() {
    jump_conditional_immediate16(!is_flag_set(FLAG_CARRY_MASK));
}

// 0xd3 is an unused opcode

void CPU::call_if_not_carry_immediate16_0xd4() {
    call_conditional_immediate16(!is_flag_set(FLAG_CARRY_MASK));
}

void CPU::push_stack_de_0xd5() {
    push_stack_register16(registers.de);
}

void CPU::subtract_a_immediate8_0xd6() {
    const uint8_t immediate8 = memory.read_8(registers.program_counter + 1);
    const bool does_half_carry_occur = (registers.a & 0x0f) < (immediate8 & 0x0f);
    const bool does_carry_occur = registers.a < immediate8;
    registers.a -= immediate8;
    update_flags(registers.a == 0, true, does_half_carry_occur, does_carry_occur);
    registers.program_counter += 2;
    cycles_elapsed += 8;
}

void CPU::restart_at_0x10_0xd7() {
    restart_at_address(0x10);
}

void CPU::return_if_carry_0xd8() {
    return_conditional(is_flag_set(FLAG_CARRY_MASK));
}

void CPU::return_from_interrupt_0xd9() {
    registers.program_counter = memory.read_16(registers.stack_pointer);
    registers.stack_pointer += 2;
    are_interrupts_enabled = true;
    cycles_elapsed += 16;
}

void CPU::jump_if_carry_immediate16_0xda() {
    jump_conditional_immediate16(is_flag_set(FLAG_CARRY_MASK));
}

// 0xdb is an unused opcode

void CPU::call_if_carry_immediate16_0xdc() {
    call_conditional_immediate16(is_flag_set(FLAG_CARRY_MASK));
}

// 0xdd is an unused opcode

void CPU::subtract_with_carry_a_immediate8_0xde() {
    const uint8_t immediate8 = memory.read_8(registers.program_counter + 1);
    const uint8_t carry_in = is_flag_set(FLAG_CARRY_MASK) ? 1 : 0;
    const bool does_half_carry_occur = (registers.a & 0x0f) < (immediate8 & 0x0f) + carry_in;
    const bool does_carry_occur = registers.a < immediate8 + carry_in;
    registers.a -= immediate8 + carry_in;
    update_flags(registers.a == 0, true, does_half_carry_occur, does_carry_occur);
    registers.program_counter += 2;
    cycles_elapsed += 8;
}

void CPU::restart_at_0x18_0xdf() {
    restart_at_address(0x18);
}

void CPU::load_memory_high_ram_offset_immediate8_a_0xe0() {
    const uint8_t unsigned_offset = memory.read_8(registers.program_counter + 1);
    memory.write_8(HIGH_RAM_START + unsigned_offset, registers.a);
    registers.program_counter += 2;
    cycles_elapsed += 12;
}

void CPU::pop_stack_hl_0xe1() {
    pop_stack_register16(registers.hl);
}

void CPU::load_memory_high_ram_c_a_0xe2() {
    memory.write_8(HIGH_RAM_START + registers.c, registers.a);
    registers.program_counter += 1;
    cycles_elapsed += 8;
}

void CPU::push_stack_hl_0xe5() {
    push_stack_register16(registers.hl);
}

void CPU::and_a_immediate8_0xe6() {
    registers.a &= memory.read_8(registers.program_counter + 1);
    update_flags(registers.a == 0, false, true, false);
    registers.program_counter += 2;
    cycles_elapsed += 8;
}

void CPU::restart_at_0x20_0xe7() {
    restart_at_address(0x20);
}

void CPU::add_sp_signed_immediate8_0xe8() {
    // Carries are based on the unsigned immediate byte while the result is based on its signed equivalent
    const uint8_t unsigned_offset = memory.read_8(registers.program_counter + 1);
    const bool does_half_carry_occur = (registers.stack_pointer & 0x0f) + (unsigned_offset & 0x0f) > 0x0f;
    const bool does_carry_occur = (registers.stack_pointer & 0xff) + (unsigned_offset & 0xff) > 0xff;
    registers.stack_pointer += static_cast<int8_t>(unsigned_offset);
    update_flags(false, false, does_half_carry_occur, does_carry_occur);
    registers.program_counter += 2;
    cycles_elapsed += 16;
}

void CPU::jump_hl_0xe9() {
    registers.program_counter = registers.hl;
    cycles_elapsed += 4;
}

void CPU::load_memory_immediate16_a_0xea() {
    const uint16_t destination_address = memory.read_16(registers.program_counter + 1);
    memory.write_8(destination_address, registers.a);
    registers.program_counter += 3;
    cycles_elapsed += 16;
}

// 0xeb is an unused opcode

// 0xec is an unused opcode

// 0xed is an unused opcode

void CPU::xor_a_immediate8_0xee() {
    registers.a ^= memory.read_8(registers.program_counter + 1);
    update_flags(registers.a == 0, false, false, false);
    registers.program_counter += 2;
    cycles_elapsed += 8;
}

void CPU::restart_at_0x28_0xef() {
    restart_at_address(0x28);
}

void CPU::load_a_memory_high_ram_immediate8_0xf0() {
    const uint8_t unsigned_offset = memory.read_8(registers.program_counter + 1);
    registers.a = memory.read_8(HIGH_RAM_START + unsigned_offset);
    registers.program_counter += 2;
    cycles_elapsed += 12;
}

void CPU::pop_stack_af_0xf1() {
    pop_stack_register16(registers.af);
    registers.flags &= 0xf0; // Lower nibble of flags must always be zeroed
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

void CPU::push_stack_af_0xf5() {
    push_stack_register16(registers.af);
}

void CPU::or_a_immediate8_0xf6() {
    registers.a |= memory.read_8(registers.program_counter + 1);
    update_flags(registers.a == 0, false, false, false);
    registers.program_counter += 2;
    cycles_elapsed += 8;
}

void CPU::restart_at_0x30_0xf7() {
    restart_at_address(0x30);
}

void CPU::load_hl_stack_pointer_with_signed_offset_0xf8() {
    // Carries are based on the unsigned immediate byte while the result is based on its signed equivalent
    const uint8_t unsigned_offset = memory.read_8(registers.program_counter + 1);
    const bool does_half_carry_occur = (registers.stack_pointer & 0x0f) + (unsigned_offset & 0x0f) > 0x0f;
    const bool does_carry_occur = (registers.stack_pointer & 0xff) + (unsigned_offset & 0xff) > 0xff;
    registers.hl = registers.stack_pointer + static_cast<int8_t>(unsigned_offset);
    update_flags(false, false, does_half_carry_occur, does_carry_occur);
    registers.program_counter += 2;
    cycles_elapsed += 12;
}

void CPU::load_stack_pointer_hl_0xf9() {
    registers.stack_pointer = registers.hl;
    registers.program_counter += 1;
    cycles_elapsed += 8;
}

void CPU::load_a_memory_immediate16_0xfa() {
    const uint16_t source_address = memory.read_16(registers.program_counter + 1);
    registers.a = memory.read_8(source_address);
    registers.program_counter += 3;
    cycles_elapsed += 16;
}

void CPU::enable_interrupts_0xfb() {
    // Enabling interrupts is delayed until after the next instruction executes
    // TODO look into this logic, may not be right
    did_enable_interrupts_execute = true;
    registers.program_counter += 1;
    cycles_elapsed += 4;
}

// 0xfc is an unused opcode

// 0xfd is an unused opcode

void CPU::compare_a_immediate8_0xfe() {
    const uint8_t immediate8 = memory.read_8(registers.program_counter + 1);
    const bool does_half_carry_occur = (registers.a & 0x0f) < (immediate8 & 0x0f);
    const bool does_carry_occur = registers.a < immediate8;
    update_flags(registers.a == immediate8, true, does_half_carry_occur, does_carry_occur);
    registers.program_counter += 2;
    cycles_elapsed += 8;
}

void CPU::restart_at_0x38_0xff() {
    restart_at_address(0x38);
}

// =================================
// ===== Extended Instructions =====
// =================================

void CPU::rotate_left_circular_b_prefix_0x00() {
    rotate_left_circular_register8(registers.b);
}

void CPU::rotate_left_circular_c_prefix_0x01() {
    rotate_left_circular_register8(registers.c);
}

void CPU::rotate_left_circular_d_prefix_0x02() {
    rotate_left_circular_register8(registers.d);
}

void CPU::rotate_left_circular_e_prefix_0x03() {
    rotate_left_circular_register8(registers.e);
}

void CPU::rotate_left_circular_h_prefix_0x04() {
    rotate_left_circular_register8(registers.h);
}

void CPU::rotate_left_circular_l_prefix_0x05() {
    rotate_left_circular_register8(registers.l);
}

void CPU::rotate_left_circular_memory_hl_prefix_0x06() {
    uint8_t memory_hl = memory.read_8(registers.hl);
    const bool does_carry_occur = (memory_hl & 0b10000000) != 0;
    memory_hl = (memory_hl << 1) | (memory_hl >> 7);
    memory.write_8(registers.hl, memory_hl);
    update_flags(memory_hl == 0, false, false, does_carry_occur);
    registers.program_counter += 2;
    cycles_elapsed += 16;
}

void CPU::rotate_left_circular_a_prefix_0x07() {
    rotate_left_circular_register8(registers.a);
}

void CPU::rotate_right_circular_b_prefix_0x08() {
    rotate_right_circular_register8(registers.b);
}

void CPU::rotate_right_circular_c_prefix_0x09() {
    rotate_right_circular_register8(registers.c);
}

void CPU::rotate_right_circular_d_prefix_0x0a() {
    rotate_right_circular_register8(registers.d);
}

void CPU::rotate_right_circular_e_prefix_0x0b() {
    rotate_right_circular_register8(registers.e);
}

void CPU::rotate_right_circular_h_prefix_0x0c() {
    rotate_right_circular_register8(registers.h);
}

void CPU::rotate_right_circular_l_prefix_0x0d() {
    rotate_right_circular_register8(registers.l);
}

void CPU::rotate_right_circular_memory_hl_prefix_0x0e() {
    uint8_t memory_hl = memory.read_8(registers.hl);
    const bool does_carry_occur = (memory_hl & 0b00000001) != 0;
    memory_hl = (memory_hl << 7) | (memory_hl >> 1);
    memory.write_8(registers.hl, memory_hl);
    update_flags(memory_hl == 0, false, false, does_carry_occur);
    registers.program_counter += 2;
    cycles_elapsed += 16;
}

void CPU::rotate_right_circular_a_prefix_0x0f() {
    rotate_right_circular_register8(registers.a);
}


void CPU::rotate_left_through_carry_b_prefix_0x10() {
    rotate_left_through_carry_register8(registers.b);
}

void CPU::rotate_left_through_carry_c_prefix_0x11() {
    rotate_left_through_carry_register8(registers.c);
}

void CPU::rotate_left_through_carry_d_prefix_0x12() {
    rotate_left_through_carry_register8(registers.d);
}

void CPU::rotate_left_through_carry_e_prefix_0x13() {
    rotate_left_through_carry_register8(registers.e);
}

void CPU::rotate_left_through_carry_h_prefix_0x14() {
    rotate_left_through_carry_register8(registers.h);
}

void CPU::rotate_left_through_carry_l_prefix_0x15() {
    rotate_left_through_carry_register8(registers.l);
}

void CPU::rotate_left_through_carry_memory_hl_prefix_0x16() {
    uint8_t memory_hl = memory.read_8(registers.hl);
    const uint8_t carry_in = is_flag_set(FLAG_CARRY_MASK) ? 1 : 0;
    const bool does_carry_occur = (memory_hl & 0b10000000) != 0;
    memory_hl = (memory_hl << 1) | carry_in;
    memory.write_8(registers.hl, memory_hl);
    update_flags(memory_hl == 0, false, false, does_carry_occur);
    registers.program_counter += 2;
    cycles_elapsed += 16;
}

void CPU::rotate_left_through_carry_a_prefix_0x17() {
    rotate_left_through_carry_register8(registers.a);
}

void CPU::rotate_right_through_carry_b_prefix_0x18() {
    rotate_right_through_carry_register8(registers.b);
}

void CPU::rotate_right_through_carry_c_prefix_0x19() {
    rotate_right_through_carry_register8(registers.c);
}

void CPU::rotate_right_through_carry_d_prefix_0x1a() {
    rotate_right_through_carry_register8(registers.d);
}

void CPU::rotate_right_through_carry_e_prefix_0x1b() {
    rotate_right_through_carry_register8(registers.e);
}

void CPU::rotate_right_through_carry_h_prefix_0x1c() {
    rotate_right_through_carry_register8(registers.h);
}

void CPU::rotate_right_through_carry_l_prefix_0x1d() {
    rotate_right_through_carry_register8(registers.l);
}

void CPU::rotate_right_through_carry_memory_hl_prefix_0x1e() {
    uint8_t memory_hl = memory.read_8(registers.hl);
    const uint8_t carry_in = is_flag_set(FLAG_CARRY_MASK) ? 1 : 0;
    const bool does_carry_occur = (memory_hl & 0b00000001) != 0;
    memory_hl = (carry_in << 7) | (memory_hl >> 1);
    memory.write_8(registers.hl, memory_hl);
    update_flags(memory_hl == 0, false, false, does_carry_occur);
    registers.program_counter += 2;
    cycles_elapsed += 16;
}

void CPU::rotate_right_through_carry_a_prefix_0x1f() {
    rotate_right_through_carry_register8(registers.a);
}

void CPU::shift_left_arithmetic_b_prefix_0x20() {
    shift_left_arithmetic_register8(registers.b);
}

void CPU::shift_left_arithmetic_c_prefix_0x21() {
    shift_left_arithmetic_register8(registers.c);
}

void CPU::shift_left_arithmetic_d_prefix_0x22() {
    shift_left_arithmetic_register8(registers.d);
}

void CPU::shift_left_arithmetic_e_prefix_0x23() {
    shift_left_arithmetic_register8(registers.e);
}

void CPU::shift_left_arithmetic_h_prefix_0x24() {
    shift_left_arithmetic_register8(registers.h);
}

void CPU::shift_left_arithmetic_l_prefix_0x25() {
    shift_left_arithmetic_register8(registers.l);
}

void CPU::shift_left_arithmetic_memory_hl_prefix_0x26() {
    uint8_t memory_hl = memory.read_8(registers.hl);
    const bool does_carry_occur = (memory_hl & 0b10000000) != 0;
    memory_hl <<= 1;
    memory.write_8(registers.hl, memory_hl);
    update_flags(memory_hl == 0, false, false, does_carry_occur);
    registers.program_counter += 2;
    cycles_elapsed += 16;
}

void CPU::shift_left_arithmetic_a_prefix_0x27() {
    shift_left_arithmetic_register8(registers.a);
}

void CPU::shift_right_arithmetic_b_prefix_0x28() {
    shift_right_arithmetic_register8(registers.b);
}

void CPU::shift_right_arithmetic_c_prefix_0x29() {
    shift_right_arithmetic_register8(registers.c);
}

void CPU::shift_right_arithmetic_d_prefix_0x2a() {
    shift_right_arithmetic_register8(registers.d);
}

void CPU::shift_right_arithmetic_e_prefix_0x2b() {
    shift_right_arithmetic_register8(registers.e);
}

void CPU::shift_right_arithmetic_h_prefix_0x2c() {
    shift_right_arithmetic_register8(registers.h);
}

void CPU::shift_right_arithmetic_l_prefix_0x2d() {
    shift_right_arithmetic_register8(registers.l);
}

void CPU::shift_right_arithmetic_memory_hl_prefix_0x2e() {
    uint8_t memory_hl = memory.read_8(registers.hl);
    const bool does_carry_occur = (memory_hl & 0b00000001) != 0;
    const uint8_t preserved_sign_bit = memory_hl & 0b10000000;
    memory_hl = preserved_sign_bit | (memory_hl >> 1);
    memory.write_8(registers.hl, memory_hl);
    update_flags(memory_hl == 0, false, false, does_carry_occur);
    registers.program_counter += 2;
    cycles_elapsed += 16;
}

void CPU::shift_right_arithmetic_a_prefix_0x2f() {
    shift_right_arithmetic_register8(registers.a);
}

void CPU::swap_nibbles_b_prefix_0x30() {
    swap_nibbles_register8(registers.b);
}

void CPU::swap_nibbles_c_prefix_0x31() {
    swap_nibbles_register8(registers.c);
}

void CPU::swap_nibbles_d_prefix_0x32() {
    swap_nibbles_register8(registers.d);
}

void CPU::swap_nibbles_e_prefix_0x33() {
    swap_nibbles_register8(registers.e);
}

void CPU::swap_nibbles_h_prefix_0x34() {
    swap_nibbles_register8(registers.h);
}

void CPU::swap_nibbles_l_prefix_0x35() {
    swap_nibbles_register8(registers.l);
}

void CPU::swap_nibbles_memory_hl_prefix_0x36() {
    uint8_t memory_hl = memory.read_8(registers.hl);
    memory_hl = ((memory_hl & 0x0f) << 4) | ((memory_hl & 0xf0) >> 4);
    memory.write_8(registers.hl, memory_hl);
    update_flags(memory_hl == 0, false, false, false);
    registers.program_counter += 2;
    cycles_elapsed += 16;
}

void CPU::swap_nibbles_a_prefix_0x37() {
    swap_nibbles_register8(registers.a);
}

void CPU::shift_right_logical_b_prefix_0x38() {
    shift_right_logical_register8(registers.b);
}

void CPU::shift_right_logical_c_prefix_0x39() {
    shift_right_logical_register8(registers.c);
}

void CPU::shift_right_logical_d_prefix_0x3a() {
    shift_right_logical_register8(registers.d);
}

void CPU::shift_right_logical_e_prefix_0x3b() {
    shift_right_logical_register8(registers.e);
}

void CPU::shift_right_logical_h_prefix_0x3c() {
    shift_right_logical_register8(registers.h);
}

void CPU::shift_right_logical_l_prefix_0x3d() {
    shift_right_logical_register8(registers.l);
}

void CPU::shift_right_logical_memory_hl_prefix_0x3e() {
    uint8_t memory_hl = memory.read_8(registers.hl);
    const bool does_carry_occur = (memory_hl & 0b00000001) != 0;
    memory_hl >>= 1;
    memory.write_8(registers.hl, memory_hl);
    update_flags(memory_hl == 0, false, false, does_carry_occur);
    registers.program_counter += 2;
    cycles_elapsed += 16;
}

void CPU::shift_right_logical_a_prefix_0x3f() {
    shift_right_logical_register8(registers.a);
}

void CPU::test_bit_0_b_prefix_0x40() {
    test_bit_position_register8(0, registers.b);
}

void CPU::test_bit_0_c_prefix_0x41() {
    test_bit_position_register8(0, registers.c);
}

void CPU::test_bit_0_d_prefix_0x42() {
    test_bit_position_register8(0, registers.d);
}

void CPU::test_bit_0_e_prefix_0x43() {
    test_bit_position_register8(0, registers.e);
}

void CPU::test_bit_0_h_prefix_0x44() {
    test_bit_position_register8(0, registers.h);
}

void CPU::test_bit_0_l_prefix_0x45() {
    test_bit_position_register8(0, registers.l);
}

void CPU::test_bit_0_memory_hl_prefix_0x46() {
    test_bit_position_memory_hl(0);
}


void CPU::test_bit_0_a_prefix_0x47() {
    test_bit_position_register8(0, registers.a);
}

void CPU::test_bit_1_b_prefix_0x48() {
    test_bit_position_register8(1, registers.b);
}
void CPU::test_bit_1_c_prefix_0x49() {
    test_bit_position_register8(1, registers.c);
}
void CPU::test_bit_1_d_prefix_0x4a() {
    test_bit_position_register8(1, registers.d);
}
void CPU::test_bit_1_e_prefix_0x4b() {
    test_bit_position_register8(1, registers.e);
}
void CPU::test_bit_1_h_prefix_0x4c() {
    test_bit_position_register8(1, registers.h);
}
void CPU::test_bit_1_l_prefix_0x4d() {
    test_bit_position_register8(1, registers.l);
}
void CPU::test_bit_1_memory_hl_prefix_0x4e() {
    test_bit_position_memory_hl(1);
}

void CPU::test_bit_1_a_prefix_0x4f() {
    test_bit_position_register8(1, registers.a);
}

void CPU::test_bit_2_b_prefix_0x50(){
    test_bit_position_register8(2, registers.b);
}

void CPU::test_bit_2_c_prefix_0x51() {
    test_bit_position_register8(2, registers.c);
}

void CPU::test_bit_2_d_prefix_0x52() {
    test_bit_position_register8(2, registers.d);
}

void CPU::test_bit_2_e_prefix_0x53() {
    test_bit_position_register8(2, registers.e);
}

void CPU::test_bit_2_h_prefix_0x54() {
    test_bit_position_register8(2, registers.h);
}

void CPU::test_bit_2_l_prefix_0x55() {
    test_bit_position_register8(2, registers.l);
}

void CPU::test_bit_2_memory_hl_prefix_0x56() {
    test_bit_position_memory_hl(2);
}

void CPU::test_bit_2_a_prefix_0x57() {
    test_bit_position_register8(2, registers.a);
}

void CPU::test_bit_3_b_prefix_0x58() {
    test_bit_position_register8(3, registers.b);
}

void CPU::test_bit_3_c_prefix_0x59() {
    test_bit_position_register8(3, registers.c);
}

void CPU::test_bit_3_d_prefix_0x5a() {
    test_bit_position_register8(3, registers.d);
}

void CPU::test_bit_3_e_prefix_0x5b() {
    test_bit_position_register8(3, registers.e);
}

void CPU::test_bit_3_h_prefix_0x5c() {
    test_bit_position_register8(3, registers.h);
}

void CPU::test_bit_3_l_prefix_0x5d() {
    test_bit_position_register8(3, registers.l);
}

void CPU::test_bit_3_memory_hl_prefix_0x5e() {
    test_bit_position_memory_hl(3);
}

void CPU::test_bit_3_a_prefix_0x5f() {
    test_bit_position_register8(3, registers.a);
}

void CPU::test_bit_4_b_prefix_0x60() {
    test_bit_position_register8(4, registers.b);
}

void CPU::test_bit_4_c_prefix_0x61() {
    test_bit_position_register8(4, registers.c);
}

void CPU::test_bit_4_d_prefix_0x62() {
    test_bit_position_register8(4, registers.d);
}

void CPU::test_bit_4_e_prefix_0x63() {
    test_bit_position_register8(4, registers.e);
}

void CPU::test_bit_4_h_prefix_0x64() {
    test_bit_position_register8(4, registers.h);
}

void CPU::test_bit_4_l_prefix_0x65() {
    test_bit_position_register8(4, registers.l);
}

void CPU::test_bit_4_memory_hl_prefix_0x66() {
    test_bit_position_memory_hl(4);
}

void CPU::test_bit_4_a_prefix_0x67() {
    test_bit_position_register8(4, registers.a);
}

void CPU::test_bit_5_b_prefix_0x68() {
    test_bit_position_register8(5, registers.b);
}

void CPU::test_bit_5_c_prefix_0x69() {
    test_bit_position_register8(5, registers.c);
}

void CPU::test_bit_5_d_prefix_0x6a() {
    test_bit_position_register8(5, registers.d);
}

void CPU::test_bit_5_e_prefix_0x6b() {
    test_bit_position_register8(5, registers.e);
}

void CPU::test_bit_5_h_prefix_0x6c() {
    test_bit_position_register8(5, registers.h);
}

void CPU::test_bit_5_l_prefix_0x6d() {
    test_bit_position_register8(5, registers.l);
}

void CPU::test_bit_5_memory_hl_prefix_0x6e() {
    test_bit_position_memory_hl(5);
}

void CPU::test_bit_5_a_prefix_0x6f() {
    test_bit_position_register8(5, registers.a);
}

void CPU::test_bit_6_b_prefix_0x70() {
    test_bit_position_register8(6, registers.b);
}

void CPU::test_bit_6_c_prefix_0x71() {
    test_bit_position_register8(6, registers.c);
}

void CPU::test_bit_6_d_prefix_0x72() {
    test_bit_position_register8(6, registers.d);
}

void CPU::test_bit_6_e_prefix_0x73() {
    test_bit_position_register8(6, registers.e);
}

void CPU::test_bit_6_h_prefix_0x74() {
    test_bit_position_register8(6, registers.h);
}

void CPU::test_bit_6_l_prefix_0x75() {
    test_bit_position_register8(6, registers.l);
}

void CPU::test_bit_6_memory_hl_prefix_0x76() {
    test_bit_position_memory_hl(6);
}

void CPU::test_bit_6_a_prefix_0x77() {
    test_bit_position_register8(6, registers.a);
}

void CPU::test_bit_7_b_prefix_0x78() {
    test_bit_position_register8(7, registers.b);
}

void CPU::test_bit_7_c_prefix_0x79() {
    test_bit_position_register8(7, registers.c);
}

void CPU::test_bit_7_d_prefix_0x7a() {
    test_bit_position_register8(7, registers.d);
}

void CPU::test_bit_7_e_prefix_0x7b() {
    test_bit_position_register8(7, registers.e);
}

void CPU::test_bit_7_h_prefix_0x7c() {
    test_bit_position_register8(7, registers.h);
}

void CPU::test_bit_7_l_prefix_0x7d() {
    test_bit_position_register8(7, registers.l);
}

void CPU::test_bit_7_memory_hl_prefix_0x7e() {
    test_bit_position_memory_hl(7);
}

void CPU::test_bit_7_a_prefix_0x7f() {
    test_bit_position_register8(7, registers.a);
}

void CPU::reset_bit_0_b_prefix_0x80() {
    reset_bit_position_register8(0, registers.b);
}

void CPU::reset_bit_0_c_prefix_0x81() {
    reset_bit_position_register8(0, registers.c);
}

void CPU::reset_bit_0_d_prefix_0x82() {
    reset_bit_position_register8(0, registers.d);
}

void CPU::reset_bit_0_e_prefix_0x83() {
    reset_bit_position_register8(0, registers.e);
}

void CPU::reset_bit_0_h_prefix_0x84() {
    reset_bit_position_register8(0, registers.h);
}

void CPU::reset_bit_0_l_prefix_0x85() {
    reset_bit_position_register8(0, registers.l);
}

void CPU::reset_bit_0_memory_hl_prefix_0x86() {
    reset_bit_position_memory_hl(0);
}

void CPU::reset_bit_0_a_prefix_0x87() {
    reset_bit_position_register8(0, registers.a);
}

void CPU::reset_bit_1_b_prefix_0x88() {
    reset_bit_position_register8(1, registers.b);
}

void CPU::reset_bit_1_c_prefix_0x89() {
    reset_bit_position_register8(1, registers.c);
}

void CPU::reset_bit_1_d_prefix_0x8a() {
    reset_bit_position_register8(1, registers.d);
}

void CPU::reset_bit_1_e_prefix_0x8b() {
    reset_bit_position_register8(1, registers.e);
}

void CPU::reset_bit_1_h_prefix_0x8c() {
    reset_bit_position_register8(1, registers.h);
}

void CPU::reset_bit_1_l_prefix_0x8d() {
    reset_bit_position_register8(1, registers.l);
}

void CPU::reset_bit_1_memory_hl_prefix_0x8e() {
    reset_bit_position_memory_hl(1);
}

void CPU::reset_bit_1_a_prefix_0x8f() {
    reset_bit_position_register8(1, registers.a);
}

void CPU::reset_bit_2_b_prefix_0x90() {
    reset_bit_position_register8(2, registers.b);
}

void CPU::reset_bit_2_c_prefix_0x91() {
    reset_bit_position_register8(2, registers.c);
}

void CPU::reset_bit_2_d_prefix_0x92() {
    reset_bit_position_register8(2, registers.d);
}

void CPU::reset_bit_2_e_prefix_0x93() {
    reset_bit_position_register8(2, registers.e);
}

void CPU::reset_bit_2_h_prefix_0x94() {
    reset_bit_position_register8(2, registers.h);
}

void CPU::reset_bit_2_l_prefix_0x95() {
    reset_bit_position_register8(2, registers.l);
}

void CPU::reset_bit_2_memory_hl_prefix_0x96() {
    reset_bit_position_memory_hl(2);
}

void CPU::reset_bit_2_a_prefix_0x97() {
    reset_bit_position_register8(2, registers.a);
}

void CPU::reset_bit_3_b_prefix_0x98() {
    reset_bit_position_register8(3, registers.b);
}

void CPU::reset_bit_3_c_prefix_0x99() {
    reset_bit_position_register8(3, registers.c);
}

void CPU::reset_bit_3_d_prefix_0x9a() {
    reset_bit_position_register8(3, registers.d);
}

void CPU::reset_bit_3_e_prefix_0x9b() {
    reset_bit_position_register8(3, registers.e);
}

void CPU::reset_bit_3_h_prefix_0x9c() {
    reset_bit_position_register8(3, registers.h);
}

void CPU::reset_bit_3_l_prefix_0x9d() {
    reset_bit_position_register8(3, registers.l);
}

void CPU::reset_bit_3_memory_hl_prefix_0x9e() {
    reset_bit_position_memory_hl(3);
}

void CPU::reset_bit_3_a_prefix_0x9f() {
    reset_bit_position_register8(3, registers.a);
}

void CPU::reset_bit_4_b_prefix_0xa0() {
    reset_bit_position_register8(4, registers.b);
}

void CPU::reset_bit_4_c_prefix_0xa1() {
    reset_bit_position_register8(4, registers.c);
}

void CPU::reset_bit_4_d_prefix_0xa2() {
    reset_bit_position_register8(4, registers.d);
}

void CPU::reset_bit_4_e_prefix_0xa3() {
    reset_bit_position_register8(4, registers.e);
}

void CPU::reset_bit_4_h_prefix_0xa4() {
    reset_bit_position_register8(4, registers.h);
}

void CPU::reset_bit_4_l_prefix_0xa5() {
    reset_bit_position_register8(4, registers.l);
}

void CPU::reset_bit_4_memory_hl_prefix_0xa6() {
    reset_bit_position_memory_hl(4);
}

void CPU::reset_bit_4_a_prefix_0xa7() {
    reset_bit_position_register8(4, registers.a);
}

void CPU::reset_bit_5_b_prefix_0xa8() {
    reset_bit_position_register8(5, registers.b);
}

void CPU::reset_bit_5_c_prefix_0xa9() {
    reset_bit_position_register8(5, registers.c);
}

void CPU::reset_bit_5_d_prefix_0xaa() {
    reset_bit_position_register8(5, registers.d);
}

void CPU::reset_bit_5_e_prefix_0xab() {
    reset_bit_position_register8(5, registers.e);
}

void CPU::reset_bit_5_h_prefix_0xac() {
    reset_bit_position_register8(5, registers.h);
}

void CPU::reset_bit_5_l_prefix_0xad() {
    reset_bit_position_register8(5, registers.l);
}

void CPU::reset_bit_5_memory_hl_prefix_0xae() {
    reset_bit_position_memory_hl(5);
}

void CPU::reset_bit_5_a_prefix_0xaf() {
    reset_bit_position_register8(5, registers.a);
}

void CPU::reset_bit_6_b_prefix_0xb0() {
    reset_bit_position_register8(6, registers.b);
}

void CPU::reset_bit_6_c_prefix_0xb1() {
    reset_bit_position_register8(6, registers.c);
}

void CPU::reset_bit_6_d_prefix_0xb2() {
    reset_bit_position_register8(6, registers.d);
}

void CPU::reset_bit_6_e_prefix_0xb3() {
    reset_bit_position_register8(6, registers.e);
}

void CPU::reset_bit_6_h_prefix_0xb4() {
    reset_bit_position_register8(6, registers.h);
}

void CPU::reset_bit_6_l_prefix_0xb5() {
    reset_bit_position_register8(6, registers.l);
}

void CPU::reset_bit_6_memory_hl_prefix_0xb6() {
    reset_bit_position_memory_hl(6);
}

void CPU::reset_bit_6_a_prefix_0xb7() {
    reset_bit_position_register8(6, registers.a);
}

void CPU::reset_bit_7_b_prefix_0xb8() {
    reset_bit_position_register8(7, registers.b);
}

void CPU::reset_bit_7_c_prefix_0xb9() {
    reset_bit_position_register8(7, registers.c);
}

void CPU::reset_bit_7_d_prefix_0xba() {
    reset_bit_position_register8(7, registers.d);
}

void CPU::reset_bit_7_e_prefix_0xbb() {
    reset_bit_position_register8(7, registers.e);
}

void CPU::reset_bit_7_h_prefix_0xbc() {
    reset_bit_position_register8(7, registers.h);
}

void CPU::reset_bit_7_l_prefix_0xbd() {
    reset_bit_position_register8(7, registers.l);
}

void CPU::reset_bit_7_memory_hl_prefix_0xbe() {
    reset_bit_position_memory_hl(7);
}

void CPU::reset_bit_7_a_prefix_0xbf() {
    reset_bit_position_register8(7, registers.a);
}

void CPU::set_bit_0_b_prefix_0xc0() {
    set_bit_position_register8(0, registers.b);
}

void CPU::set_bit_0_c_prefix_0xc1() {
    set_bit_position_register8(0, registers.c);
}

void CPU::set_bit_0_d_prefix_0xc2() {
    set_bit_position_register8(0, registers.d);
}

void CPU::set_bit_0_e_prefix_0xc3() {
    set_bit_position_register8(0, registers.e);
}

void CPU::set_bit_0_h_prefix_0xc4() {
    set_bit_position_register8(0, registers.h);
}

void CPU::set_bit_0_l_prefix_0xc5() {
    set_bit_position_register8(0, registers.l);
}

void CPU::set_bit_0_memory_hl_prefix_0xc6() {
    set_bit_position_memory_hl(0);
}

void CPU::set_bit_0_a_prefix_0xc7() {
    set_bit_position_register8(0, registers.a);
}

void CPU::set_bit_1_b_prefix_0xc8() {
    set_bit_position_register8(1, registers.b);
}

void CPU::set_bit_1_c_prefix_0xc9() {
    set_bit_position_register8(1, registers.c);
}

void CPU::set_bit_1_d_prefix_0xca() {
    set_bit_position_register8(1, registers.d);
}

void CPU::set_bit_1_e_prefix_0xcb() {
    set_bit_position_register8(1, registers.e);
}

void CPU::set_bit_1_h_prefix_0xcc() {
    set_bit_position_register8(1, registers.h);
}

void CPU::set_bit_1_l_prefix_0xcd() {
    set_bit_position_register8(1, registers.l);
}

void CPU::set_bit_1_memory_hl_prefix_0xce() {
    set_bit_position_memory_hl(1);
}

void CPU::set_bit_1_a_prefix_0xcf() {
    set_bit_position_register8(1, registers.a);
}

void CPU::set_bit_2_b_prefix_0xd0() {
    set_bit_position_register8(2, registers.b);
}

void CPU::set_bit_2_c_prefix_0xd1() {
    set_bit_position_register8(2, registers.c);
}

void CPU::set_bit_2_d_prefix_0xd2() {
    set_bit_position_register8(2, registers.d);
}

void CPU::set_bit_2_e_prefix_0xd3() {
    set_bit_position_register8(2, registers.e);
}

void CPU::set_bit_2_h_prefix_0xd4() {
    set_bit_position_register8(2, registers.h);
}

void CPU::set_bit_2_l_prefix_0xd5() {
    set_bit_position_register8(2, registers.l);
}

void CPU::set_bit_2_memory_hl_prefix_0xd6() {
    set_bit_position_memory_hl(2);
}

void CPU::set_bit_2_a_prefix_0xd7() {
    set_bit_position_register8(2, registers.a);
}

void CPU::set_bit_3_b_prefix_0xd8() {
    set_bit_position_register8(3, registers.b);
}

void CPU::set_bit_3_c_prefix_0xd9() {
    set_bit_position_register8(3, registers.c);
}

void CPU::set_bit_3_d_prefix_0xda() {
    set_bit_position_register8(3, registers.d);
}

void CPU::set_bit_3_e_prefix_0xdb() {
    set_bit_position_register8(3, registers.e);
}

void CPU::set_bit_3_h_prefix_0xdc() {
    set_bit_position_register8(3, registers.h);
}

void CPU::set_bit_3_l_prefix_0xdd() {
    set_bit_position_register8(3, registers.l);
}

void CPU::set_bit_3_memory_hl_prefix_0xde() {
    set_bit_position_memory_hl(3);
}

void CPU::set_bit_3_a_prefix_0xdf() {
    set_bit_position_register8(3, registers.a);
}

void CPU::set_bit_4_b_prefix_0xe0() {
    set_bit_position_register8(4, registers.b);
}

void CPU::set_bit_4_c_prefix_0xe1() {
    set_bit_position_register8(4, registers.c);
}

void CPU::set_bit_4_d_prefix_0xe2() {
    set_bit_position_register8(4, registers.d);
}

void CPU::set_bit_4_e_prefix_0xe3() {
    set_bit_position_register8(4, registers.e);
}

void CPU::set_bit_4_h_prefix_0xe4() {
    set_bit_position_register8(4, registers.h);
}

void CPU::set_bit_4_l_prefix_0xe5() {
    set_bit_position_register8(4, registers.l);
}

void CPU::set_bit_4_memory_hl_prefix_0xe6() {
    set_bit_position_memory_hl(4);
}

void CPU::set_bit_4_a_prefix_0xe7() {
    set_bit_position_register8(4, registers.a);
}

void CPU::set_bit_5_b_prefix_0xe8() {
    set_bit_position_register8(5, registers.b);
}

void CPU::set_bit_5_c_prefix_0xe9() {
    set_bit_position_register8(5, registers.c);
}

void CPU::set_bit_5_d_prefix_0xea() {
    set_bit_position_register8(5, registers.d);
}

void CPU::set_bit_5_e_prefix_0xeb() {
    set_bit_position_register8(5, registers.e);
}

void CPU::set_bit_5_h_prefix_0xec() {
    set_bit_position_register8(5, registers.h);
}

void CPU::set_bit_5_l_prefix_0xed() {
    set_bit_position_register8(5, registers.l);
}

void CPU::set_bit_5_memory_hl_prefix_0xee() {
    set_bit_position_memory_hl(5);
}

void CPU::set_bit_5_a_prefix_0xef() {
    set_bit_position_register8(5, registers.a);
}

void CPU::set_bit_6_b_prefix_0xf0() {
    set_bit_position_register8(6, registers.b);
}

void CPU::set_bit_6_c_prefix_0xf1() {
    set_bit_position_register8(6, registers.c);
}

void CPU::set_bit_6_d_prefix_0xf2() {
    set_bit_position_register8(6, registers.d);
}

void CPU::set_bit_6_e_prefix_0xf3() {
    set_bit_position_register8(6, registers.e);
}

void CPU::set_bit_6_h_prefix_0xf4() {
    set_bit_position_register8(6, registers.h);
}

void CPU::set_bit_6_l_prefix_0xf5() {
    set_bit_position_register8(6, registers.l);
}

void CPU::set_bit_6_memory_hl_prefix_0xf6() {
    set_bit_position_memory_hl(6);
}

void CPU::set_bit_6_a_prefix_0xf7() {
    set_bit_position_register8(6, registers.a);
}

void CPU::set_bit_7_b_prefix_0xf8() {
    set_bit_position_register8(7, registers.b);
}

void CPU::set_bit_7_c_prefix_0xf9() {
    set_bit_position_register8(7, registers.c);
}

void CPU::set_bit_7_d_prefix_0xfa() {
    set_bit_position_register8(7, registers.d);
}

void CPU::set_bit_7_e_prefix_0xfb() {
    set_bit_position_register8(7, registers.e);
}

void CPU::set_bit_7_h_prefix_0xfc() {
    set_bit_position_register8(7, registers.h);
}

void CPU::set_bit_7_l_prefix_0xfd() {
    set_bit_position_register8(7, registers.l);
}

void CPU::set_bit_7_memory_hl_prefix_0xfe() {
    set_bit_position_memory_hl(7);
}

void CPU::set_bit_7_a_prefix_0xff() {
    set_bit_position_register8(7, registers.a);
}

} // namespace GameBoy
