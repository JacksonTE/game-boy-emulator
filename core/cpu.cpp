#include <bit>
#include <iomanip>
#include <iostream>
#include "cpu.h"
#include "register_file.h"

namespace GameBoy {

CPU::CPU(MemoryManagementUnit &memory_management_unit, std::function<void(MachineCycleInteraction)> tick_callback)
    : emulator_tick_callback{tick_callback},
      memory_interface{memory_management_unit} {
}

void CPU::reset_state() {
    set_register_file_state(RegisterFile<std::endian::native>{});
    cycles_elapsed = 0;
    interrupt_master_enable_ime = InterruptMasterEnableState::Disabled;
    instruction_register_ir = 0x00;
    is_instruction_prefixed = false;
    is_stopped = false;
    is_halted = false;
}

void CPU::set_post_boot_state() {
    // Updates flags based on header bytes 0x0134-0x014c in the loaded 'cartridge' ROM
    uint8_t header_checksum = 0;
    for (uint16_t address = 0x0134; address <= 0x014c; address++) {
        header_checksum -= memory_interface.read_byte(BOOTROM_SIZE + address) - 1;
    }

    register_file.a = 0x01;
    update_flag(FLAG_ZERO_MASK, true);
    update_flag(FLAG_SUBTRACT_MASK, false);
    update_flag(FLAG_HALF_CARRY_MASK, header_checksum != 0);
    update_flag(FLAG_CARRY_MASK, header_checksum != 0);
    register_file.b = 0x00;
    register_file.c = 0x13;
    register_file.d = 0x00;
    register_file.e = 0xd8;
    register_file.h = 0x01;
    register_file.l = 0x4d;
    register_file.program_counter = 0x100;
    register_file.stack_pointer = 0xfffe;
}

void CPU::step() {
    if (is_halted) {
        emulator_tick_callback(MachineCycleInteraction{MemoryOperation::None});
    }
    else {
        execute_next_instruction_and_fetch();
    }
    service_interrupt();
    if (interrupt_master_enable_ime == InterruptMasterEnableState::WillEnable) {
        interrupt_master_enable_ime = InterruptMasterEnableState::Enabled;
    }
}

void CPU::tick_machine_cycle() {
    cycles_elapsed += 4; // Operations typically take multiples of 4 CPU cycles to complete, called machine cycles (or M-cycles)
}

RegisterFile<std::endian::native> CPU::get_register_file() const {
    return register_file;
}

void CPU::set_register_file_state(const RegisterFile<std::endian::native> &new_register_values) {
    register_file.a = new_register_values.a;
    register_file.flags = new_register_values.flags & 0xf0; // Lower nibble of flags must always be zeroed
    register_file.bc = new_register_values.bc;
    register_file.de = new_register_values.de;
    register_file.hl = new_register_values.hl;
    register_file.program_counter = new_register_values.program_counter;
    register_file.stack_pointer = new_register_values.stack_pointer;
}

void CPU::print_register_file_state() const {
    std::cout << "=================== CPU Registers ===================\n";
    std::cout << std::hex << std::setfill('0');

    std::cout << "AF: 0x" << std::setw(4) << register_file.af << "   "
              << "(A: 0x" << std::setw(2) << static_cast<int>(register_file.a) << ","
              << " F: 0x" << std::setw(2) << static_cast<int>(register_file.flags) << ")   "
              << "Flags ZNHC: " << (is_flag_set(FLAG_ZERO_MASK) ? "1" : "0")
                                << (is_flag_set(FLAG_SUBTRACT_MASK) ? "1" : "0")
                                << (is_flag_set(FLAG_HALF_CARRY_MASK) ? "1" : "0")
                                << (is_flag_set(FLAG_CARRY_MASK) ? "1" : "0") << "\n";

    std::cout << "BC: 0x" << std::setw(4) << register_file.bc << "   "
              << "(B: 0x" << std::setw(2) << static_cast<int>(register_file.b) << ","
              << " C: 0x" << std::setw(2) << static_cast<int>(register_file.c) << ")\n";

    std::cout << "DE: 0x" << std::setw(4) << register_file.de << "   "
              << "(D: 0x" << std::setw(2) << static_cast<int>(register_file.d) << ","
              << " E: 0x" << std::setw(2) << static_cast<int>(register_file.e) << ")\n";

    std::cout << "HL: 0x" << std::setw(4) << register_file.hl << "   "
              << "(H: 0x" << std::setw(2) << static_cast<int>(register_file.h) << ","
              << " L: 0x" << std::setw(2) << static_cast<int>(register_file.l) << ")\n";

    std::cout << "Stack Pointer: 0x" << std::setw(4) << register_file.stack_pointer << "\n";
    std::cout << "Program Counter: 0x" << std::setw(4) << register_file.program_counter << "\n";

    std::cout << std::dec;
    std::cout << "Cycles Elapsed: " << cycles_elapsed << "\n";
    std::cout << "=====================================================\n";
}

void CPU::execute_next_instruction_and_fetch() {
    if (is_instruction_prefixed) {
        (this->*extended_instruction_table[instruction_register_ir])();
    }
    else {
        (this->*instruction_table[instruction_register_ir])();
    }

    uint8_t immediate8 = fetch_immediate8_and_tick();
    is_instruction_prefixed = (immediate8 == INSTRUCTION_PREFIX_BYTE);
    instruction_register_ir = is_instruction_prefixed
        ? fetch_immediate8_and_tick()
        : immediate8;
}

void CPU::service_interrupt() {
    auto x = get_pending_interrupt_mask();
    bool is_interrupt_pending = (get_pending_interrupt_mask() != 0);
    if (is_interrupt_pending) {
        is_halted = false;
    }

    if (interrupt_master_enable_ime != InterruptMasterEnableState::Enabled || !is_interrupt_pending) {
        return;
    }

    decrement_register16(register_file.program_counter);
    decrement_register16(register_file.stack_pointer);
    write_byte_and_tick(register_file.stack_pointer--, register_file.program_counter >> 8);
    uint8_t interrupt_flag_mask = get_pending_interrupt_mask();
    write_byte_and_tick(register_file.stack_pointer, register_file.program_counter & 0xff);

    memory_interface.clear_interrupt_flag_bit(interrupt_flag_mask);
    interrupt_master_enable_ime = InterruptMasterEnableState::Disabled;

    register_file.program_counter = (interrupt_flag_mask == 0x00)
        ? 0x00
        : 0x0040 + 8 * static_cast<uint8_t>(std::countr_zero(interrupt_flag_mask));

    uint8_t immediate8 = fetch_immediate8_and_tick();
    is_instruction_prefixed = (immediate8 == INSTRUCTION_PREFIX_BYTE);
    instruction_register_ir = is_instruction_prefixed
        ? fetch_immediate8_and_tick()
        : immediate8;
}

uint8_t CPU::get_pending_interrupt_mask() {
    for (uint8_t i = 0; i < NUMBER_OF_INTERRUPT_TYPES; i++) {
        uint8_t interrupt_flag_mask = 1 << i;
        if (memory_interface.is_interrupt_type_requested(interrupt_flag_mask) &&
            memory_interface.is_interrupt_type_enabled(interrupt_flag_mask)) {
            return interrupt_flag_mask;
        }
    }
    return 0x00;
}

uint8_t CPU::read_byte_and_tick(uint16_t address) {
    emulator_tick_callback(MachineCycleInteraction{MemoryOperation::Read, address});
    return memory_interface.read_byte(address);
}

void CPU::write_byte_and_tick(uint16_t address, uint8_t value) {
    emulator_tick_callback(MachineCycleInteraction{MemoryOperation::Write, address, value});
    memory_interface.write_byte(address, value);
}

uint8_t CPU::fetch_immediate8_and_tick() {
    uint8_t immediate8 = read_byte_and_tick(register_file.program_counter);
    if (!is_halted) {
        register_file.program_counter++;
    }
    return immediate8;
}

uint16_t CPU::fetch_immediate16_and_tick() {
    const uint8_t low_byte = fetch_immediate8_and_tick();
    return low_byte | static_cast<uint16_t>(fetch_immediate8_and_tick() << 8);
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
    &CPU::load_memory_high_ram_offset_c_a_0xe2,
    &CPU::unused_opcode,
    &CPU::unused_opcode,
    &CPU::push_stack_hl_0xe5,
    &CPU::and_a_immediate8_0xe6,
    &CPU::restart_at_0x20_0xe7,
    &CPU::add_stack_pointer_signed_immediate8_0xe8,
    &CPU::jump_hl_0xe9,
    &CPU::load_memory_immediate16_a_0xea,
    &CPU::unused_opcode, // 0xeb is an unused opcode
    &CPU::unused_opcode, // 0xec is an unused opcode
    &CPU::unused_opcode, // 0xed is an unused opcode
    &CPU::xor_a_immediate8_0xee,
    &CPU::restart_at_0x28_0xef,
    &CPU::load_a_memory_high_ram_offset_immediate8_0xf0,
    &CPU::pop_stack_af_0xf1,
    &CPU::load_a_memory_high_ram_offset_c_0xf2,
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
    &CPU::rotate_left_circular_b_0xcb_0x00,
    &CPU::rotate_left_circular_c_0xcb_0x01,
    &CPU::rotate_left_circular_d_0xcb_0x02,
    &CPU::rotate_left_circular_e_0xcb_0x03,
    &CPU::rotate_left_circular_h_0xcb_0x04,
    &CPU::rotate_left_circular_l_0xcb_0x05,
    &CPU::rotate_left_circular_memory_hl_0xcb_0x06,
    &CPU::rotate_left_circular_a_0xcb_0x07,
    &CPU::rotate_right_circular_b_0xcb_0x08,
    &CPU::rotate_right_circular_c_0xcb_0x09,
    &CPU::rotate_right_circular_d_0xcb_0x0a,
    &CPU::rotate_right_circular_e_0xcb_0x0b,
    &CPU::rotate_right_circular_h_0xcb_0x0c,
    &CPU::rotate_right_circular_l_0xcb_0x0d,
    &CPU::rotate_right_circular_memory_hl_0xcb_0x0e,
    &CPU::rotate_right_circular_a_0xcb_0x0f,
    &CPU::rotate_left_through_carry_b_0xcb_0x10,
    &CPU::rotate_left_through_carry_c_0xcb_0x11,
    &CPU::rotate_left_through_carry_d_0xcb_0x12,
    &CPU::rotate_left_through_carry_e_0xcb_0x13,
    &CPU::rotate_left_through_carry_h_0xcb_0x14,
    &CPU::rotate_left_through_carry_l_0xcb_0x15,
    &CPU::rotate_left_through_carry_memory_hl_0xcb_0x16,
    &CPU::rotate_left_through_carry_a_0xcb_0x17,
    &CPU::rotate_right_through_carry_b_0xcb_0x18,
    &CPU::rotate_right_through_carry_c_0xcb_0x19,
    &CPU::rotate_right_through_carry_d_0xcb_0x1a,
    &CPU::rotate_right_through_carry_e_0xcb_0x1b,
    &CPU::rotate_right_through_carry_h_0xcb_0x1c,
    &CPU::rotate_right_through_carry_l_0xcb_0x1d,
    &CPU::rotate_right_through_carry_memory_hl_0xcb_0x1e,
    &CPU::rotate_right_through_carry_a_0xcb_0x1f,
    &CPU::shift_left_arithmetic_b_0xcb_0x20,
    &CPU::shift_left_arithmetic_c_0xcb_0x21,
    &CPU::shift_left_arithmetic_d_0xcb_0x22,
    &CPU::shift_left_arithmetic_e_0xcb_0x23,
    &CPU::shift_left_arithmetic_h_0xcb_0x24,
    &CPU::shift_left_arithmetic_l_0xcb_0x25,
    &CPU::shift_left_arithmetic_memory_hl_0xcb_0x26,
    &CPU::shift_left_arithmetic_a_0xcb_0x27,
    &CPU::shift_right_arithmetic_b_0xcb_0x28,
    &CPU::shift_right_arithmetic_c_0xcb_0x29,
    &CPU::shift_right_arithmetic_d_0xcb_0x2a,
    &CPU::shift_right_arithmetic_e_0xcb_0x2b,
    &CPU::shift_right_arithmetic_h_0xcb_0x2c,
    &CPU::shift_right_arithmetic_l_0xcb_0x2d,
    &CPU::shift_right_arithmetic_memory_hl_0xcb_0x2e,
    &CPU::shift_right_arithmetic_a_0xcb_0x2f,
    &CPU::swap_nibbles_b_0xcb_0x30,
    &CPU::swap_nibbles_c_0xcb_0x31,
    &CPU::swap_nibbles_d_0xcb_0x32,
    &CPU::swap_nibbles_e_0xcb_0x33,
    &CPU::swap_nibbles_h_0xcb_0x34,
    &CPU::swap_nibbles_l_0xcb_0x35,
    &CPU::swap_nibbles_memory_hl_0xcb_0x36,
    &CPU::swap_nibbles_a_0xcb_0x37,
    &CPU::shift_right_logical_b_0xcb_0x38,
    &CPU::shift_right_logical_c_0xcb_0x39,
    &CPU::shift_right_logical_d_0xcb_0x3a,
    &CPU::shift_right_logical_e_0xcb_0x3b,
    &CPU::shift_right_logical_h_0xcb_0x3c,
    &CPU::shift_right_logical_l_0xcb_0x3d,
    &CPU::shift_right_logical_memory_hl_0xcb_0x3e,
    &CPU::shift_right_logical_a_0xcb_0x3f,
    &CPU::test_bit_0_b_0xcb_0x40,
    &CPU::test_bit_0_c_0xcb_0x41,
    &CPU::test_bit_0_d_0xcb_0x42,
    &CPU::test_bit_0_e_0xcb_0x43,
    &CPU::test_bit_0_h_0xcb_0x44,
    &CPU::test_bit_0_l_0xcb_0x45,
    &CPU::test_bit_0_memory_hl_0xcb_0x46,
    &CPU::test_bit_0_a_0xcb_0x47,
    &CPU::test_bit_1_b_0xcb_0x48,
    &CPU::test_bit_1_c_0xcb_0x49,
    &CPU::test_bit_1_d_0xcb_0x4a,
    &CPU::test_bit_1_e_0xcb_0x4b,
    &CPU::test_bit_1_h_0xcb_0x4c,
    &CPU::test_bit_1_l_0xcb_0x4d,
    &CPU::test_bit_1_memory_hl_0xcb_0x4e,
    &CPU::test_bit_1_a_0xcb_0x4f,
    &CPU::test_bit_2_b_0xcb_0x50,
    &CPU::test_bit_2_c_0xcb_0x51,
    &CPU::test_bit_2_d_0xcb_0x52,
    &CPU::test_bit_2_e_0xcb_0x53,
    &CPU::test_bit_2_h_0xcb_0x54,
    &CPU::test_bit_2_l_0xcb_0x55,
    &CPU::test_bit_2_memory_hl_0xcb_0x56,
    &CPU::test_bit_2_a_0xcb_0x57,
    &CPU::test_bit_3_b_0xcb_0x58,
    &CPU::test_bit_3_c_0xcb_0x59,
    &CPU::test_bit_3_d_0xcb_0x5a,
    &CPU::test_bit_3_e_0xcb_0x5b,
    &CPU::test_bit_3_h_0xcb_0x5c,
    &CPU::test_bit_3_l_0xcb_0x5d,
    &CPU::test_bit_3_memory_hl_0xcb_0x5e,
    &CPU::test_bit_3_a_0xcb_0x5f,
    &CPU::test_bit_4_b_0xcb_0x60,
    &CPU::test_bit_4_c_0xcb_0x61,
    &CPU::test_bit_4_d_0xcb_0x62,
    &CPU::test_bit_4_e_0xcb_0x63,
    &CPU::test_bit_4_h_0xcb_0x64,
    &CPU::test_bit_4_l_0xcb_0x65,
    &CPU::test_bit_4_memory_hl_0xcb_0x66,
    &CPU::test_bit_4_a_0xcb_0x67,
    &CPU::test_bit_5_b_0xcb_0x68,
    &CPU::test_bit_5_c_0xcb_0x69,
    &CPU::test_bit_5_d_0xcb_0x6a,
    &CPU::test_bit_5_e_0xcb_0x6b,
    &CPU::test_bit_5_h_0xcb_0x6c,
    &CPU::test_bit_5_l_0xcb_0x6d,
    &CPU::test_bit_5_memory_hl_0xcb_0x6e,
    &CPU::test_bit_5_a_0xcb_0x6f,
    &CPU::test_bit_6_b_0xcb_0x70,
    &CPU::test_bit_6_c_0xcb_0x71,
    &CPU::test_bit_6_d_0xcb_0x72,
    &CPU::test_bit_6_e_0xcb_0x73,
    &CPU::test_bit_6_h_0xcb_0x74,
    &CPU::test_bit_6_l_0xcb_0x75,
    &CPU::test_bit_6_memory_hl_0xcb_0x76,
    &CPU::test_bit_6_a_0xcb_0x77,
    &CPU::test_bit_7_b_0xcb_0x78,
    &CPU::test_bit_7_c_0xcb_0x79,
    &CPU::test_bit_7_d_0xcb_0x7a,
    &CPU::test_bit_7_e_0xcb_0x7b,
    &CPU::test_bit_7_h_0xcb_0x7c,
    &CPU::test_bit_7_l_0xcb_0x7d,
    &CPU::test_bit_7_memory_hl_0xcb_0x7e,
    &CPU::test_bit_7_a_0xcb_0x7f,
    &CPU::reset_bit_0_b_0xcb_0x80,
    &CPU::reset_bit_0_c_0xcb_0x81,
    &CPU::reset_bit_0_d_0xcb_0x82,
    &CPU::reset_bit_0_e_0xcb_0x83,
    &CPU::reset_bit_0_h_0xcb_0x84,
    &CPU::reset_bit_0_l_0xcb_0x85,
    &CPU::reset_bit_0_memory_hl_0xcb_0x86,
    &CPU::reset_bit_0_a_0xcb_0x87,
    &CPU::reset_bit_1_b_0xcb_0x88,
    &CPU::reset_bit_1_c_0xcb_0x89,
    &CPU::reset_bit_1_d_0xcb_0x8a,
    &CPU::reset_bit_1_e_0xcb_0x8b,
    &CPU::reset_bit_1_h_0xcb_0x8c,
    &CPU::reset_bit_1_l_0xcb_0x8d,
    &CPU::reset_bit_1_memory_hl_0xcb_0x8e,
    &CPU::reset_bit_1_a_0xcb_0x8f,
    &CPU::reset_bit_2_b_0xcb_0x90,
    &CPU::reset_bit_2_c_0xcb_0x91,
    &CPU::reset_bit_2_d_0xcb_0x92,
    &CPU::reset_bit_2_e_0xcb_0x93,
    &CPU::reset_bit_2_h_0xcb_0x94,
    &CPU::reset_bit_2_l_0xcb_0x95,
    &CPU::reset_bit_2_memory_hl_0xcb_0x96,
    &CPU::reset_bit_2_a_0xcb_0x97,
    &CPU::reset_bit_3_b_0xcb_0x98,
    &CPU::reset_bit_3_c_0xcb_0x99,
    &CPU::reset_bit_3_d_0xcb_0x9a,
    &CPU::reset_bit_3_e_0xcb_0x9b,
    &CPU::reset_bit_3_h_0xcb_0x9c,
    &CPU::reset_bit_3_l_0xcb_0x9d,
    &CPU::reset_bit_3_memory_hl_0xcb_0x9e,
    &CPU::reset_bit_3_a_0xcb_0x9f,
    &CPU::reset_bit_4_b_0xcb_0xa0,
    &CPU::reset_bit_4_c_0xcb_0xa1,
    &CPU::reset_bit_4_d_0xcb_0xa2,
    &CPU::reset_bit_4_e_0xcb_0xa3,
    &CPU::reset_bit_4_h_0xcb_0xa4,
    &CPU::reset_bit_4_l_0xcb_0xa5,
    &CPU::reset_bit_4_memory_hl_0xcb_0xa6,
    &CPU::reset_bit_4_a_0xcb_0xa7,
    &CPU::reset_bit_5_b_0xcb_0xa8,
    &CPU::reset_bit_5_c_0xcb_0xa9,
    &CPU::reset_bit_5_d_0xcb_0xaa,
    &CPU::reset_bit_5_e_0xcb_0xab,
    &CPU::reset_bit_5_h_0xcb_0xac,
    &CPU::reset_bit_5_l_0xcb_0xad,
    &CPU::reset_bit_5_memory_hl_0xcb_0xae,
    &CPU::reset_bit_5_a_0xcb_0xaf,
    &CPU::reset_bit_6_b_0xcb_0xb0,
    &CPU::reset_bit_6_c_0xcb_0xb1,
    &CPU::reset_bit_6_d_0xcb_0xb2,
    &CPU::reset_bit_6_e_0xcb_0xb3,
    &CPU::reset_bit_6_h_0xcb_0xb4,
    &CPU::reset_bit_6_l_0xcb_0xb5,
    &CPU::reset_bit_6_memory_hl_0xcb_0xb6,
    &CPU::reset_bit_6_a_0xcb_0xb7,
    &CPU::reset_bit_7_b_0xcb_0xb8,
    &CPU::reset_bit_7_c_0xcb_0xb9,
    &CPU::reset_bit_7_d_0xcb_0xba,
    &CPU::reset_bit_7_e_0xcb_0xbb,
    &CPU::reset_bit_7_h_0xcb_0xbc,
    &CPU::reset_bit_7_l_0xcb_0xbd,
    &CPU::reset_bit_7_memory_hl_0xcb_0xbe,
    &CPU::reset_bit_7_a_0xcb_0xbf,
    &CPU::set_bit_0_b_0xcb_0xc0,
    &CPU::set_bit_0_c_0xcb_0xc1,
    &CPU::set_bit_0_d_0xcb_0xc2,
    &CPU::set_bit_0_e_0xcb_0xc3,
    &CPU::set_bit_0_h_0xcb_0xc4,
    &CPU::set_bit_0_l_0xcb_0xc5,
    &CPU::set_bit_0_memory_hl_0xcb_0xc6,
    &CPU::set_bit_0_a_0xcb_0xc7,
    &CPU::set_bit_1_b_0xcb_0xc8,
    &CPU::set_bit_1_c_0xcb_0xc9,
    &CPU::set_bit_1_d_0xcb_0xca,
    &CPU::set_bit_1_e_0xcb_0xcb,
    &CPU::set_bit_1_h_0xcb_0xcc,
    &CPU::set_bit_1_l_0xcb_0xcd,
    &CPU::set_bit_1_memory_hl_0xcb_0xce,
    &CPU::set_bit_1_a_0xcb_0xcf,
    &CPU::set_bit_2_b_0xcb_0xd0,
    &CPU::set_bit_2_c_0xcb_0xd1,
    &CPU::set_bit_2_d_0xcb_0xd2,
    &CPU::set_bit_2_e_0xcb_0xd3,
    &CPU::set_bit_2_h_0xcb_0xd4,
    &CPU::set_bit_2_l_0xcb_0xd5,
    &CPU::set_bit_2_memory_hl_0xcb_0xd6,
    &CPU::set_bit_2_a_0xcb_0xd7,
    &CPU::set_bit_3_b_0xcb_0xd8,
    &CPU::set_bit_3_c_0xcb_0xd9,
    &CPU::set_bit_3_d_0xcb_0xda,
    &CPU::set_bit_3_e_0xcb_0xdb,
    &CPU::set_bit_3_h_0xcb_0xdc,
    &CPU::set_bit_3_l_0xcb_0xdd,
    &CPU::set_bit_3_memory_hl_0xcb_0xde,
    &CPU::set_bit_3_a_0xcb_0xdf,
    &CPU::set_bit_4_b_0xcb_0xe0,
    &CPU::set_bit_4_c_0xcb_0xe1,
    &CPU::set_bit_4_d_0xcb_0xe2,
    &CPU::set_bit_4_e_0xcb_0xe3,
    &CPU::set_bit_4_h_0xcb_0xe4,
    &CPU::set_bit_4_l_0xcb_0xe5,
    &CPU::set_bit_4_memory_hl_0xcb_0xe6,
    &CPU::set_bit_4_a_0xcb_0xe7,
    &CPU::set_bit_5_b_0xcb_0xe8,
    &CPU::set_bit_5_c_0xcb_0xe9,
    &CPU::set_bit_5_d_0xcb_0xea,
    &CPU::set_bit_5_e_0xcb_0xeb,
    &CPU::set_bit_5_h_0xcb_0xec,
    &CPU::set_bit_5_l_0xcb_0xed,
    &CPU::set_bit_5_memory_hl_0xcb_0xee,
    &CPU::set_bit_5_a_0xcb_0xef,
    &CPU::set_bit_6_b_0xcb_0xf0,
    &CPU::set_bit_6_c_0xcb_0xf1,
    &CPU::set_bit_6_d_0xcb_0xf2,
    &CPU::set_bit_6_e_0xcb_0xf3,
    &CPU::set_bit_6_h_0xcb_0xf4,
    &CPU::set_bit_6_l_0xcb_0xf5,
    &CPU::set_bit_6_memory_hl_0xcb_0xf6,
    &CPU::set_bit_6_a_0xcb_0xf7,
    &CPU::set_bit_7_b_0xcb_0xf8,
    &CPU::set_bit_7_c_0xcb_0xf9,
    &CPU::set_bit_7_d_0xcb_0xfa,
    &CPU::set_bit_7_e_0xcb_0xfb,
    &CPU::set_bit_7_h_0xcb_0xfc,
    &CPU::set_bit_7_l_0xcb_0xfd,
    &CPU::set_bit_7_memory_hl_0xcb_0xfe,
    &CPU::set_bit_7_a_0xcb_0xff
};

// ===============================
// ===== Instruction Helpers =====
// ===============================

void CPU::update_flag(uint8_t flag_mask, bool new_flag_state) {
    register_file.flags = new_flag_state
        ? (register_file.flags | flag_mask)
        : (register_file.flags & ~flag_mask);
}

bool CPU::is_flag_set(uint8_t flag_mask) const {
    return (register_file.flags & flag_mask) != 0;
}

void CPU::increment_register16(uint16_t &register16) {
    emulator_tick_callback(MachineCycleInteraction{MemoryOperation::None});
    register16++;
}

void CPU::decrement_register16(uint16_t &register16) {
    emulator_tick_callback(MachineCycleInteraction{MemoryOperation::None});
    register16--;
}

void CPU::add_hl_register16(const uint16_t &register16) {
    emulator_tick_callback(MachineCycleInteraction{MemoryOperation::None});
    const bool does_half_carry_occur = (register_file.hl & 0x0fff) + (register16 & 0x0fff) > 0x0fff;
    const bool does_carry_occur = static_cast<uint32_t>(register_file.hl) + register16 > 0xffff;
    register_file.hl += register16;
    update_flag(FLAG_SUBTRACT_MASK, false);
    update_flag(FLAG_HALF_CARRY_MASK, does_half_carry_occur);
    update_flag(FLAG_CARRY_MASK, does_carry_occur);
}

void CPU::increment_uint8(uint8_t &uint8) {
    const bool does_half_carry_occur = (uint8 & 0x0f) == 0x0f;
    uint8++;
    update_flag(FLAG_ZERO_MASK, uint8 == 0);
    update_flag(FLAG_SUBTRACT_MASK, false);
    update_flag(FLAG_HALF_CARRY_MASK, does_half_carry_occur);
}

void CPU::decrement_uint8(uint8_t &uint8) {
    const bool does_half_carry_occur = (uint8 & 0x0f) == 0x00;
    uint8--;
    update_flag(FLAG_ZERO_MASK, uint8 == 0);
    update_flag(FLAG_SUBTRACT_MASK, true);
    update_flag(FLAG_HALF_CARRY_MASK, does_half_carry_occur);
}

void CPU::add_a_uint8(const uint8_t &uint8) {
    const bool does_half_carry_occur = (register_file.a & 0x0f) + (uint8 & 0x0f) > 0x0f;
    const bool does_carry_occur = static_cast<uint16_t>(register_file.a) + uint8 > 0xff;
    register_file.a += uint8;
    update_flag(FLAG_ZERO_MASK, register_file.a == 0);
    update_flag(FLAG_SUBTRACT_MASK, false);
    update_flag(FLAG_HALF_CARRY_MASK, does_half_carry_occur);
    update_flag(FLAG_CARRY_MASK, does_carry_occur);
}

void CPU::add_with_carry_a_uint8(const uint8_t &uint8) {
    const uint8_t carry_in = is_flag_set(FLAG_CARRY_MASK) ? 1 : 0;
    const bool does_half_carry_occur = (register_file.a & 0x0f) + (uint8 & 0x0f) + carry_in > 0x0f;
    const bool does_carry_occur = static_cast<uint16_t>(register_file.a) + uint8 + carry_in > 0xff;
    register_file.a += uint8 + carry_in;
    update_flag(FLAG_ZERO_MASK, register_file.a == 0);
    update_flag(FLAG_SUBTRACT_MASK, false);
    update_flag(FLAG_HALF_CARRY_MASK, does_half_carry_occur);
    update_flag(FLAG_CARRY_MASK, does_carry_occur);
}

void CPU::subtract_a_uint8(const uint8_t &uint8) {
    const bool does_half_carry_occur = (register_file.a & 0x0f) < (uint8 & 0x0f);
    const bool does_carry_occur = register_file.a < uint8;
    register_file.a -= uint8;
    update_flag(FLAG_ZERO_MASK, register_file.a == 0);
    update_flag(FLAG_SUBTRACT_MASK, true);
    update_flag(FLAG_HALF_CARRY_MASK, does_half_carry_occur);
    update_flag(FLAG_CARRY_MASK, does_carry_occur);
}

void CPU::subtract_with_carry_a_uint8(const uint8_t &uint8) {
    const uint8_t carry_in = is_flag_set(FLAG_CARRY_MASK) ? 1 : 0;
    const bool does_half_carry_occur = (register_file.a & 0x0f) < (uint8 & 0x0f) + carry_in;
    const bool does_carry_occur = register_file.a < uint8 + carry_in;
    register_file.a -= uint8 + carry_in;
    update_flag(FLAG_ZERO_MASK, register_file.a == 0);
    update_flag(FLAG_SUBTRACT_MASK, true);
    update_flag(FLAG_HALF_CARRY_MASK, does_half_carry_occur);
    update_flag(FLAG_CARRY_MASK, does_carry_occur);
}

void CPU::and_a_uint8(const uint8_t &uint8) {
    register_file.a &= uint8;
    update_flag(FLAG_ZERO_MASK, register_file.a == 0);
    update_flag(FLAG_SUBTRACT_MASK, false);
    update_flag(FLAG_HALF_CARRY_MASK, true);
    update_flag(FLAG_CARRY_MASK, false);
}

void CPU::xor_a_uint8(const uint8_t &uint8) {
    register_file.a ^= uint8;
    update_flag(FLAG_ZERO_MASK, register_file.a == 0);
    update_flag(FLAG_SUBTRACT_MASK, false);
    update_flag(FLAG_HALF_CARRY_MASK, false);
    update_flag(FLAG_CARRY_MASK, false);
}

void CPU::or_a_uint8(const uint8_t &uint8) {
    register_file.a |= uint8;
    update_flag(FLAG_ZERO_MASK, register_file.a == 0);
    update_flag(FLAG_SUBTRACT_MASK, false);
    update_flag(FLAG_HALF_CARRY_MASK, false);
    update_flag(FLAG_CARRY_MASK, false);
}

void CPU::compare_a_uint8(const uint8_t &uint8) {
    const bool does_half_carry_occur = (register_file.a & 0x0f) < (uint8 & 0x0f);
    const bool does_carry_occur = register_file.a < uint8;
    update_flag(FLAG_ZERO_MASK, register_file.a == uint8);
    update_flag(FLAG_SUBTRACT_MASK, true);
    update_flag(FLAG_HALF_CARRY_MASK, does_half_carry_occur);
    update_flag(FLAG_CARRY_MASK, does_carry_occur);
}

void CPU::jump_relative_conditional_signed_immediate8(bool is_condition_met) {
    const int8_t signed_offset = static_cast<int8_t>(fetch_immediate8_and_tick());
    if (is_condition_met) {
        emulator_tick_callback(MachineCycleInteraction{MemoryOperation::None});
        register_file.program_counter += signed_offset;
    }
}

void CPU::jump_conditional_immediate16(bool is_condition_met) {
    const uint16_t jump_address = fetch_immediate16_and_tick();
    if (is_condition_met) {
        emulator_tick_callback(MachineCycleInteraction{MemoryOperation::None});
        register_file.program_counter = jump_address;
    }
}

uint16_t CPU::pop_stack() {
    uint8_t low_byte = read_byte_and_tick(register_file.stack_pointer++);
    return low_byte | static_cast<uint16_t>(read_byte_and_tick(register_file.stack_pointer++) << 8);
}

void CPU::push_stack_register16(const uint16_t &value) {
    decrement_register16(register_file.stack_pointer);
    write_byte_and_tick(register_file.stack_pointer--, value >> 8);
    write_byte_and_tick(register_file.stack_pointer, value & 0xff);
}

void CPU::call_conditional_immediate16(bool is_condition_met) {
    const uint16_t subroutine_address = fetch_immediate16_and_tick();
    if (is_condition_met) {
        restart_at_address(subroutine_address);
    }
}

void CPU::return_conditional(bool is_condition_met) {
    emulator_tick_callback(MachineCycleInteraction{MemoryOperation::None});
    if (is_condition_met) {
        return_0xc9();
    }
}

void CPU::restart_at_address(uint16_t address) {
    push_stack_register16(register_file.program_counter);
    register_file.program_counter = address;
}

void CPU::rotate_left_circular_uint8(uint8_t &uint8) {
    const bool does_carry_occur = (uint8 & 0b10000000) != 0;
    uint8 = (uint8 << 1) | (uint8 >> 7);
    update_flag(FLAG_ZERO_MASK, uint8 == 0);
    update_flag(FLAG_SUBTRACT_MASK, false);
    update_flag(FLAG_HALF_CARRY_MASK, false);
    update_flag(FLAG_CARRY_MASK, does_carry_occur);
}

void CPU::rotate_right_circular_uint8(uint8_t &uint8) {
    const bool does_carry_occur = (uint8 & 0b00000001) != 0;
    uint8 = (uint8 << 7) | (uint8 >> 1);
    update_flag(FLAG_ZERO_MASK, uint8 == 0);
    update_flag(FLAG_SUBTRACT_MASK, false);
    update_flag(FLAG_HALF_CARRY_MASK, false);
    update_flag(FLAG_CARRY_MASK, does_carry_occur);
}

void CPU::rotate_left_through_carry_uint8(uint8_t &uint8) {
    const uint8_t carry_in = is_flag_set(FLAG_CARRY_MASK) ? 1 : 0;
    const bool does_carry_occur = (uint8 & 0b10000000) != 0;
    uint8 = (uint8 << 1) | carry_in;
    update_flag(FLAG_ZERO_MASK, uint8 == 0);
    update_flag(FLAG_SUBTRACT_MASK, false);
    update_flag(FLAG_HALF_CARRY_MASK, false);
    update_flag(FLAG_CARRY_MASK, does_carry_occur);
}

void CPU::rotate_right_through_carry_uint8(uint8_t &uint8) {
    const uint8_t carry_in = is_flag_set(FLAG_CARRY_MASK) ? 1 : 0;
    const bool does_carry_occur = (uint8 & 0b00000001) != 0;
    uint8 = (carry_in << 7) | (uint8 >> 1);
    update_flag(FLAG_ZERO_MASK, uint8 == 0);
    update_flag(FLAG_SUBTRACT_MASK, false);
    update_flag(FLAG_HALF_CARRY_MASK, false);
    update_flag(FLAG_CARRY_MASK, does_carry_occur);
}

void CPU::shift_left_arithmetic_uint8(uint8_t &uint8) {
    const bool does_carry_occur = (uint8 & 0b10000000) != 0;
    uint8 <<= 1;
    update_flag(FLAG_ZERO_MASK, uint8 == 0);
    update_flag(FLAG_SUBTRACT_MASK, false);
    update_flag(FLAG_HALF_CARRY_MASK, false);
    update_flag(FLAG_CARRY_MASK, does_carry_occur);
}

void CPU::shift_right_arithmetic_uint8(uint8_t &uint8) {
    const bool does_carry_occur = (uint8 & 0b00000001) != 0;
    const uint8_t preserved_sign_bit = uint8 & 0b10000000;
    uint8 = preserved_sign_bit | (uint8 >> 1);
    update_flag(FLAG_ZERO_MASK, uint8 == 0);
    update_flag(FLAG_SUBTRACT_MASK, false);
    update_flag(FLAG_HALF_CARRY_MASK, false);
    update_flag(FLAG_CARRY_MASK, does_carry_occur);
}

void CPU::swap_nibbles_uint8(uint8_t &uint8) {
    uint8 = ((uint8 & 0x0f) << 4) | ((uint8 & 0xf0) >> 4);
    update_flag(FLAG_ZERO_MASK, uint8 == 0);
    update_flag(FLAG_SUBTRACT_MASK, false);
    update_flag(FLAG_HALF_CARRY_MASK, false);
    update_flag(FLAG_CARRY_MASK, false);
}

void CPU::shift_right_logical_uint8(uint8_t &uint8) {
    const bool does_carry_occur = (uint8 & 0b00000001) != 0;
    uint8 >>= 1;
    update_flag(FLAG_ZERO_MASK, uint8 == 0);
    update_flag(FLAG_SUBTRACT_MASK, false);
    update_flag(FLAG_HALF_CARRY_MASK, false);
    update_flag(FLAG_CARRY_MASK, does_carry_occur);
}

void CPU::test_bit_position_uint8(uint8_t bit_position_to_test, const uint8_t &uint8) {
    const bool is_bit_set = (uint8 & (1 << bit_position_to_test)) != 0;
    update_flag(FLAG_ZERO_MASK, !is_bit_set);
    update_flag(FLAG_SUBTRACT_MASK, false);
    update_flag(FLAG_HALF_CARRY_MASK, true);
}

void CPU::test_bit_position_memory_hl(uint8_t bit_position_to_test) {
    uint8_t memory_hl = read_byte_and_tick(register_file.hl);
    test_bit_position_uint8(bit_position_to_test, memory_hl);
}

void CPU::reset_bit_position_uint8(uint8_t bit_position_to_reset, uint8_t &uint8) {
    uint8 &= ~(1 << bit_position_to_reset);
}

void CPU::reset_bit_position_memory_hl(uint8_t bit_position_to_reset) {
    uint8_t memory_hl = read_byte_and_tick(register_file.hl);
    reset_bit_position_uint8(bit_position_to_reset, memory_hl);
    write_byte_and_tick(register_file.hl, memory_hl);
}

void CPU::set_bit_position_uint8(uint8_t bit_position_to_set, uint8_t &uint8) {
    uint8 |= (1 << bit_position_to_set);
}

void CPU::set_bit_position_memory_hl(uint8_t bit_position_to_set) {
    uint8_t memory_hl = read_byte_and_tick(register_file.hl);
    set_bit_position_uint8(bit_position_to_set, memory_hl);
    write_byte_and_tick(register_file.hl, memory_hl);
}

// ========================
// ===== Instructions =====
// ========================

void CPU::unused_opcode() {
    std::cerr << std::hex << std::setfill('0');
    std::cerr << "Warning: Unused opcode 0x" << std::setw(2) 
              << static_cast<int>(memory_interface.read_byte(register_file.program_counter - 1)) << " "
              << "encountered at memory address 0x" << std::setw(2) 
              << static_cast<int>(register_file.program_counter - 1) << "\n";
}

void CPU::no_operation_0x00() {
}

void CPU::load_bc_immediate16_0x01() {
    register_file.bc = fetch_immediate16_and_tick();
}

void CPU::load_memory_bc_a_0x02() {
    write_byte_and_tick(register_file.bc, register_file.a);
}

void CPU::increment_bc_0x03() {
    increment_register16(register_file.bc);
}

void CPU::increment_b_0x04() {
    increment_uint8(register_file.b);
}

void CPU::decrement_b_0x05() {
    decrement_uint8(register_file.b);
}

void CPU::load_b_immediate8_0x06() {
    register_file.b = fetch_immediate8_and_tick();
}

void CPU::rotate_left_circular_a_0x07() {
    rotate_left_circular_uint8(register_file.a);
    update_flag(FLAG_ZERO_MASK, false);
}

void CPU::load_memory_immediate16_stack_pointer_0x08() {
    uint16_t immediate16 = fetch_immediate16_and_tick();
    const uint8_t stack_pointer_low_byte = static_cast<uint8_t>(register_file.stack_pointer & 0xff);
    const uint8_t stack_pointer_high_byte = static_cast<uint8_t>(register_file.stack_pointer >> 8);
    write_byte_and_tick(immediate16, stack_pointer_low_byte);
    write_byte_and_tick(immediate16 + 1, stack_pointer_high_byte);
}

void CPU::add_hl_bc_0x09() {
    add_hl_register16(register_file.bc);
}

void CPU::load_a_memory_bc_0x0a() {
    register_file.a = read_byte_and_tick(register_file.bc);
}

void CPU::decrement_bc_0x0b() {
    decrement_register16(register_file.bc);
}

void CPU::increment_c_0x0c() {
    increment_uint8(register_file.c);
}

void CPU::decrement_c_0x0d() {
    decrement_uint8(register_file.c);
}

void CPU::load_c_immediate8_0x0e() {
    register_file.c = fetch_immediate8_and_tick();
}

void CPU::rotate_right_circular_a_0x0f() {
    rotate_right_circular_uint8(register_file.a);
    update_flag(FLAG_ZERO_MASK, false);
}

void CPU::stop_0x10() {
    // TODO implement this properly eventually
    is_stopped = true;
}

void CPU::load_de_immediate16_0x11() {
    register_file.de = fetch_immediate16_and_tick();
}

void CPU::load_memory_de_a_0x12() {
    write_byte_and_tick(register_file.de, register_file.a);
}

void CPU::increment_de_0x13() {
    increment_register16(register_file.de);
}

void CPU::increment_d_0x14() {
    increment_uint8(register_file.d);
}

void CPU::decrement_d_0x15() {
    decrement_uint8(register_file.d);
}

void CPU::load_d_immediate8_0x16() {
    register_file.d = fetch_immediate8_and_tick();
}

void CPU::rotate_left_through_carry_a_0x17() {
    rotate_left_through_carry_uint8(register_file.a);
    update_flag(FLAG_ZERO_MASK, false);
}

void CPU::jump_relative_signed_immediate8_0x18() {
    jump_relative_conditional_signed_immediate8(true);
}

void CPU::add_hl_de_0x19() {
    add_hl_register16(register_file.de);
}

void CPU::load_a_memory_de_0x1a() {
    register_file.a = read_byte_and_tick(register_file.de);
}

void CPU::decrement_de_0x1b() {
    decrement_register16(register_file.de);
}

void CPU::increment_e_0x1c() {
    increment_uint8(register_file.e);
}

void CPU::decrement_e_0x1d() {
    decrement_uint8(register_file.e);
}

void CPU::load_e_immediate8_0x1e() {
    register_file.e = fetch_immediate8_and_tick();
}

void CPU::rotate_right_through_carry_a_0x1f() {
    rotate_right_through_carry_uint8(register_file.a);
    update_flag(FLAG_ZERO_MASK, false);
}

void CPU::jump_relative_if_not_zero_signed_immediate8_0x20() {
    jump_relative_conditional_signed_immediate8(!is_flag_set(FLAG_ZERO_MASK));
}

void CPU::load_hl_immediate16_0x21() {
    register_file.hl = fetch_immediate16_and_tick();
}

void CPU::load_memory_post_increment_hl_a_0x22() {
    write_byte_and_tick(register_file.hl++, register_file.a);
}

void CPU::increment_hl_0x23() {
    increment_register16(register_file.hl);
}

void CPU::increment_h_0x24() {
    increment_uint8(register_file.h);
}

void CPU::decrement_h_0x25() {
    decrement_uint8(register_file.h);
}

void CPU::load_h_immediate8_0x26() {
    register_file.h = fetch_immediate8_and_tick();
}

void CPU::decimal_adjust_a_0x27() {
    const bool was_addition_most_recent = !is_flag_set(FLAG_SUBTRACT_MASK);
    bool does_carry_occur = false;
    uint8_t adjustment = 0;
    // Previous operation was between two binary coded decimals (BCDs) and this corrects register A back to BCD format
    if (is_flag_set(FLAG_HALF_CARRY_MASK) || (was_addition_most_recent && (register_file.a & 0x0f) > 0x09)) {
        adjustment |= 0x06;
    }
    if (is_flag_set(FLAG_CARRY_MASK) || (was_addition_most_recent && register_file.a > 0x99)) {
        adjustment |= 0x60;
        does_carry_occur = true;
    }
    register_file.a = was_addition_most_recent
        ? (register_file.a + adjustment)
        : (register_file.a - adjustment);
    update_flag(FLAG_ZERO_MASK, register_file.a == 0);
    update_flag(FLAG_HALF_CARRY_MASK, false);
    update_flag(FLAG_CARRY_MASK, does_carry_occur);
}

void CPU::jump_relative_if_zero_signed_immediate8_0x28() {
    jump_relative_conditional_signed_immediate8(is_flag_set(FLAG_ZERO_MASK));
}

void CPU::add_hl_hl_0x29() {
    add_hl_register16(register_file.hl);
}

void CPU::load_a_memory_post_increment_hl_0x2a() {
    register_file.a = read_byte_and_tick(register_file.hl++);
}

void CPU::decrement_hl_0x2b() {
    decrement_register16(register_file.hl);
}

void CPU::increment_l_0x2c() {
    increment_uint8(register_file.l);
}

void CPU::decrement_l_0x2d() {
    decrement_uint8(register_file.l);
}

void CPU::load_l_immediate8_0x2e() {
    register_file.l = fetch_immediate8_and_tick();
}

void CPU::complement_a_0x2f() {
    register_file.a = ~register_file.a;
    update_flag(FLAG_SUBTRACT_MASK, true);
    update_flag(FLAG_HALF_CARRY_MASK, true);
}

void CPU::jump_relative_if_not_carry_signed_immediate8_0x30() {
    jump_relative_conditional_signed_immediate8(!is_flag_set(FLAG_CARRY_MASK));
}

void CPU::load_stack_pointer_immediate16_0x31() {
    register_file.stack_pointer = fetch_immediate16_and_tick();
}

void CPU::load_memory_post_decrement_hl_a_0x32() {
    write_byte_and_tick(register_file.hl--, register_file.a);
}

void CPU::increment_stack_pointer_0x33() {
    increment_register16(register_file.stack_pointer);
}

void CPU::increment_memory_hl_0x34() {
    uint8_t memory_hl = read_byte_and_tick(register_file.hl);
    increment_uint8(memory_hl);
    write_byte_and_tick(register_file.hl, memory_hl);
}

void CPU::decrement_memory_hl_0x35() {
    uint8_t memory_hl = read_byte_and_tick(register_file.hl);
    decrement_uint8(memory_hl);
    write_byte_and_tick(register_file.hl, memory_hl);
}

void CPU::load_memory_hl_immediate8_0x36() {
    write_byte_and_tick(register_file.hl, fetch_immediate8_and_tick());
}

void CPU::set_carry_flag_0x37() {
    update_flag(FLAG_SUBTRACT_MASK, false);
    update_flag(FLAG_HALF_CARRY_MASK, false);
    update_flag(FLAG_CARRY_MASK, true);
}

void CPU::jump_relative_if_carry_signed_immediate8_0x38() {
    jump_relative_conditional_signed_immediate8(is_flag_set(FLAG_CARRY_MASK));
}

void CPU::add_hl_stack_pointer_0x39() {
    add_hl_register16(register_file.stack_pointer);
}

void CPU::load_a_memory_post_decrement_hl_0x3a() {
    register_file.a = read_byte_and_tick(register_file.hl--);
}

void CPU::decrement_stack_pointer_0x3b() {
    decrement_register16(register_file.stack_pointer);
}

void CPU::increment_a_0x3c() {
    increment_uint8(register_file.a);
}

void CPU::decrement_a_0x3d() {
    decrement_uint8(register_file.a);
}

void CPU::load_a_immediate8_0x3e() {
    register_file.a = fetch_immediate8_and_tick();
}

void CPU::complement_carry_flag_0x3f() {
    update_flag(FLAG_SUBTRACT_MASK, false);
    update_flag(FLAG_HALF_CARRY_MASK, false);
    update_flag(FLAG_CARRY_MASK, !is_flag_set(FLAG_CARRY_MASK));
}

void CPU::load_b_b_0x40() {
    register_file.b = register_file.b; // Essentially no operation
}

void CPU::load_b_c_0x41() {
    register_file.b = register_file.c;
}

void CPU::load_b_d_0x42() {
    register_file.b = register_file.d;
}

void CPU::load_b_e_0x43() {
    register_file.b = register_file.e;
}

void CPU::load_b_h_0x44() {
    register_file.b = register_file.h;
}

void CPU::load_b_l_0x45() {
    register_file.b = register_file.l;
}

void CPU::load_b_memory_hl_0x46() {
    register_file.b = read_byte_and_tick(register_file.hl);
}

void CPU::load_b_a_0x47() {
    register_file.b = register_file.a;
}

void CPU::load_c_b_0x48() {
    register_file.c = register_file.b;
}

void CPU::load_c_c_0x49() {
    register_file.c = register_file.c; // Essentially no operation
}

void CPU::load_c_d_0x4a() {
    register_file.c = register_file.d;
}

void CPU::load_c_e_0x4b() {
    register_file.c = register_file.e;
}

void CPU::load_c_h_0x4c() {
    register_file.c = register_file.h;
}

void CPU::load_c_l_0x4d() {
    register_file.c = register_file.l;
}

void CPU::load_c_memory_hl_0x4e() {
    register_file.c = read_byte_and_tick(register_file.hl);
}

void CPU::load_c_a_0x4f() {
    register_file.c = register_file.a;
}

void CPU::load_d_b_0x50() {
    register_file.d = register_file.b;
}

void CPU::load_d_c_0x51() {
    register_file.d = register_file.c;
}

void CPU::load_d_d_0x52() {
    register_file.d = register_file.d; // Essentially no operation
}

void CPU::load_d_e_0x53() {
    register_file.d = register_file.e;
}

void CPU::load_d_h_0x54() {
    register_file.d = register_file.h;
}

void CPU::load_d_l_0x55() {
    register_file.d = register_file.l;
}

void CPU::load_d_memory_hl_0x56() {
    register_file.d = read_byte_and_tick(register_file.hl);
}

void CPU::load_d_a_0x57() {
    register_file.d = register_file.a;
}

void CPU::load_e_b_0x58() {
    register_file.e = register_file.b;
}

void CPU::load_e_c_0x59() {
    register_file.e = register_file.c;
}

void CPU::load_e_d_0x5a() {
    register_file.e = register_file.d;
}

void CPU::load_e_e_0x5b() {
    register_file.e = register_file.e; // Essentially no operation
}

void CPU::load_e_h_0x5c() {
    register_file.e = register_file.h;
}

void CPU::load_e_l_0x5d() {
    register_file.e = register_file.l;
}

void CPU::load_e_memory_hl_0x5e() {
    register_file.e = read_byte_and_tick(register_file.hl);
}

void CPU::load_e_a_0x5f() {
    register_file.e = register_file.a;
}

void CPU::load_h_b_0x60() {
    register_file.h = register_file.b;
}

void CPU::load_h_c_0x61() {
    register_file.h = register_file.c;
}

void CPU::load_h_d_0x62() {
    register_file.h = register_file.d;
}

void CPU::load_h_e_0x63() {
    register_file.h = register_file.e;
}

void CPU::load_h_h_0x64() {
    register_file.h = register_file.h; // Essentially no operation
}

void CPU::load_h_l_0x65() {
    register_file.h = register_file.l;
}

void CPU::load_h_memory_hl_0x66() {
    register_file.h = read_byte_and_tick(register_file.hl);
}

void CPU::load_h_a_0x67() {
    register_file.h = register_file.a;
}

void CPU::load_l_b_0x68() {
    register_file.l = register_file.b;
}

void CPU::load_l_c_0x69() {
    register_file.l = register_file.c;
}

void CPU::load_l_d_0x6a() {
    register_file.l = register_file.d;
}

void CPU::load_l_e_0x6b() {
    register_file.l = register_file.e;
}

void CPU::load_l_h_0x6c() {
    register_file.l = register_file.h;
}

void CPU::load_l_l_0x6d() {
    register_file.l = register_file.l; // Essentially no operation
}

void CPU::load_l_memory_hl_0x6e() {
    register_file.l = read_byte_and_tick(register_file.hl);
}

void CPU::load_l_a_0x6f() {
    register_file.l = register_file.a;
}

void CPU::load_memory_hl_b_0x70() {
    write_byte_and_tick(register_file.hl, register_file.b);
}

void CPU::load_memory_hl_c_0x71() {
    write_byte_and_tick(register_file.hl, register_file.c);
}

void CPU::load_memory_hl_d_0x72() {
    write_byte_and_tick(register_file.hl, register_file.d);
}

void CPU::load_memory_hl_e_0x73() {
    write_byte_and_tick(register_file.hl, register_file.e);
}

void CPU::load_memory_hl_h_0x74() { 
    write_byte_and_tick(register_file.hl, register_file.h);
}

void CPU::load_memory_hl_l_0x75() {
    write_byte_and_tick(register_file.hl, register_file.l);
}

void CPU::halt_0x76() {
    is_halted = true;
}

void CPU::load_memory_hl_a_0x77() {
    write_byte_and_tick(register_file.hl, register_file.a);
}

void CPU::load_a_b_0x78() {
    register_file.a = register_file.b;
}

void CPU::load_a_c_0x79() {
    register_file.a = register_file.c;
}

void CPU::load_a_d_0x7a() {
    register_file.a = register_file.d;
}

void CPU::load_a_e_0x7b() {
    register_file.a = register_file.e;
}

void CPU::load_a_h_0x7c() {
    register_file.a = register_file.h;
}

void CPU::load_a_l_0x7d() {
    register_file.a = register_file.l;
}

void CPU::load_a_memory_hl_0x7e() {
    register_file.a = read_byte_and_tick(register_file.hl);
}

void CPU::load_a_a_0x7f() {
    register_file.a = register_file.a; // Essentially no operation
}

void CPU::add_a_b_0x80() {
    add_a_uint8(register_file.b);
}

void CPU::add_a_c_0x81() {
    add_a_uint8(register_file.c);
}

void CPU::add_a_d_0x82() {
    add_a_uint8(register_file.d);
}

void CPU::add_a_e_0x83() {
    add_a_uint8(register_file.e);
}

void CPU::add_a_h_0x84() {
    add_a_uint8(register_file.h);
}

void CPU::add_a_l_0x85() {
    add_a_uint8(register_file.l);
}

void CPU::add_a_memory_hl_0x86() {
    add_a_uint8(read_byte_and_tick(register_file.hl));
}

void CPU::add_a_a_0x87() {
    add_a_uint8(register_file.a);
}

void CPU::add_with_carry_a_b_0x88() {
    add_with_carry_a_uint8(register_file.b);
}

void CPU::add_with_carry_a_c_0x89() {
    add_with_carry_a_uint8(register_file.c);
}

void CPU::add_with_carry_a_d_0x8a() {
    add_with_carry_a_uint8(register_file.d);
}

void CPU::add_with_carry_a_e_0x8b() {
    add_with_carry_a_uint8(register_file.e);
}

void CPU::add_with_carry_a_h_0x8c() {
    add_with_carry_a_uint8(register_file.h);
}

void CPU::add_with_carry_a_l_0x8d() {
    add_with_carry_a_uint8(register_file.l);
}

void CPU::add_with_carry_a_memory_hl_0x8e() {
    add_with_carry_a_uint8(read_byte_and_tick(register_file.hl));
}

void CPU::add_with_carry_a_a_0x8f() {
    add_with_carry_a_uint8(register_file.a);
}

void CPU::subtract_a_b_0x90() {
    subtract_a_uint8(register_file.b);
}

void CPU::subtract_a_c_0x91() {
    subtract_a_uint8(register_file.c);
}

void CPU::subtract_a_d_0x92() {
    subtract_a_uint8(register_file.d);
}

void CPU::subtract_a_e_0x93() {
    subtract_a_uint8(register_file.e);
}

void CPU::subtract_a_h_0x94() {
    subtract_a_uint8(register_file.h);
}

void CPU::subtract_a_l_0x95() {
    subtract_a_uint8(register_file.l);
}

void CPU::subtract_a_memory_hl_0x96() {
    subtract_a_uint8(read_byte_and_tick(register_file.hl));
}

void CPU::subtract_a_a_0x97() {
    subtract_a_uint8(register_file.a);
}

void CPU::subtract_with_carry_a_b_0x98() {
    subtract_with_carry_a_uint8(register_file.b);
}

void CPU::subtract_with_carry_a_c_0x99() {
    subtract_with_carry_a_uint8(register_file.c);
}

void CPU::subtract_with_carry_a_d_0x9a() {
    subtract_with_carry_a_uint8(register_file.d);
}

void CPU::subtract_with_carry_a_e_0x9b() {
    subtract_with_carry_a_uint8(register_file.e);
}

void CPU::subtract_with_carry_a_h_0x9c() {
    subtract_with_carry_a_uint8(register_file.h);
}

void CPU::subtract_with_carry_a_l_0x9d() {
    subtract_with_carry_a_uint8(register_file.l);
}

void CPU::subtract_with_carry_a_memory_hl_0x9e() {
    subtract_with_carry_a_uint8(read_byte_and_tick(register_file.hl));
}

void CPU::subtract_with_carry_a_a_0x9f() {
    subtract_with_carry_a_uint8(register_file.a);
}

void CPU::and_a_b_0xa0() {
    and_a_uint8(register_file.b);
}

void CPU::and_a_c_0xa1() {
    and_a_uint8(register_file.c);
}

void CPU::and_a_d_0xa2() {
    and_a_uint8(register_file.d);
 }

void CPU::and_a_e_0xa3() {
    and_a_uint8(register_file.e);
}

void CPU::and_a_h_0xa4() {
    and_a_uint8(register_file.h);
}

void CPU::and_a_l_0xa5() {
    and_a_uint8(register_file.l);
}

void CPU::and_a_memory_hl_0xa6() {
    and_a_uint8(read_byte_and_tick(register_file.hl));
}

void CPU::and_a_a_0xa7() {
    and_a_uint8(register_file.a);
}

void CPU::xor_a_b_0xa8() {
    xor_a_uint8(register_file.b);
}

void CPU::xor_a_c_0xa9() {
    xor_a_uint8(register_file.c);
}

void CPU::xor_a_d_0xaa() {
    xor_a_uint8(register_file.d);
}

void CPU::xor_a_e_0xab() {
    xor_a_uint8(register_file.e);
}

void CPU::xor_a_h_0xac() {
    xor_a_uint8(register_file.h);
}

void CPU::xor_a_l_0xad() {
    xor_a_uint8(register_file.l);
}

void CPU::xor_a_memory_hl_0xae() {
    xor_a_uint8(read_byte_and_tick(register_file.hl));
}

void CPU::xor_a_a_0xaf() {
    xor_a_uint8(register_file.a);
}

void CPU::or_a_b_0xb0() {
    or_a_uint8(register_file.b);
}

void CPU::or_a_c_0xb1() {
    or_a_uint8(register_file.c);
}

void CPU::or_a_d_0xb2() {
    or_a_uint8(register_file.d);
}

void CPU::or_a_e_0xb3() {
    or_a_uint8(register_file.e);
}

void CPU::or_a_h_0xb4() {
    or_a_uint8(register_file.h);
}

void CPU::or_a_l_0xb5() {
    or_a_uint8(register_file.l);
}

void CPU::or_a_memory_hl_0xb6() {
    or_a_uint8(read_byte_and_tick(register_file.hl));
}

void CPU::or_a_a_0xb7() {
    or_a_uint8(register_file.a);
}

void CPU::compare_a_b_0xb8() {
    compare_a_uint8(register_file.b);
}

void CPU::compare_a_c_0xb9() {
    compare_a_uint8(register_file.c);
}

void CPU::compare_a_d_0xba() {
    compare_a_uint8(register_file.d);
}

void CPU::compare_a_e_0xbb() {
    compare_a_uint8(register_file.e);
}

void CPU::compare_a_h_0xbc() {
    compare_a_uint8(register_file.h);
}

void CPU::compare_a_l_0xbd() {
    compare_a_uint8(register_file.l);
}

void CPU::compare_a_memory_hl_0xbe() {
    compare_a_uint8(read_byte_and_tick(register_file.hl));
}

void CPU::compare_a_a_0xbf() {
    compare_a_uint8(register_file.a);
}

void CPU::return_if_not_zero_0xc0() {
    return_conditional(!is_flag_set(FLAG_ZERO_MASK));
}

void CPU::pop_stack_bc_0xc1() {
    register_file.bc = pop_stack();
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
    push_stack_register16(register_file.bc);
}

void CPU::add_a_immediate8_0xc6() {
    add_a_uint8(fetch_immediate8_and_tick());
}

void CPU::restart_at_0x00_0xc7() {
    restart_at_address(0x00);
}

void CPU::return_if_zero_0xc8() {
    return_conditional(is_flag_set(FLAG_ZERO_MASK));
}

void CPU::return_0xc9() {
    uint16_t stack_top = pop_stack();
    emulator_tick_callback(MachineCycleInteraction{MemoryOperation::None});
    register_file.program_counter = stack_top;
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
    add_with_carry_a_uint8(fetch_immediate8_and_tick());
}

void CPU::restart_at_0x08_0xcf() {
    restart_at_address(0x08);
}

void CPU::return_if_not_carry_0xd0() {
    return_conditional(!is_flag_set(FLAG_CARRY_MASK));
}

void CPU::pop_stack_de_0xd1() {
    register_file.de = pop_stack();
}

void CPU::jump_if_not_carry_immediate16_0xd2() {
    jump_conditional_immediate16(!is_flag_set(FLAG_CARRY_MASK));
}

// 0xd3 is an unused opcode

void CPU::call_if_not_carry_immediate16_0xd4() {
    call_conditional_immediate16(!is_flag_set(FLAG_CARRY_MASK));
}

void CPU::push_stack_de_0xd5() {
    push_stack_register16(register_file.de);
}

void CPU::subtract_a_immediate8_0xd6() {
    subtract_a_uint8(fetch_immediate8_and_tick());
}

void CPU::restart_at_0x10_0xd7() {
    restart_at_address(0x10);
}

void CPU::return_if_carry_0xd8() {
    return_conditional(is_flag_set(FLAG_CARRY_MASK));
}

void CPU::return_from_interrupt_0xd9() {
    interrupt_master_enable_ime = InterruptMasterEnableState::WillEnable;
    return_0xc9();
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
    subtract_with_carry_a_uint8(fetch_immediate8_and_tick());
}

void CPU::restart_at_0x18_0xdf() {
    restart_at_address(0x18);
}

void CPU::load_memory_high_ram_offset_immediate8_a_0xe0() {
    write_byte_and_tick(HIGH_RAM_START + fetch_immediate8_and_tick(), register_file.a);
}

void CPU::pop_stack_hl_0xe1() {
    register_file.hl = pop_stack();
}

void CPU::load_memory_high_ram_offset_c_a_0xe2() {
    write_byte_and_tick(HIGH_RAM_START + register_file.c, register_file.a);
}

void CPU::push_stack_hl_0xe5() {
    push_stack_register16(register_file.hl);
}

void CPU::and_a_immediate8_0xe6() {
    and_a_uint8(fetch_immediate8_and_tick());
}

void CPU::restart_at_0x20_0xe7() {
    restart_at_address(0x20);
}

void CPU::add_stack_pointer_signed_immediate8_0xe8() {
    // Carries are based on the unsigned immediate byte while the result is based on its signed equivalent
    const uint8_t unsigned_offset = fetch_immediate8_and_tick();
    emulator_tick_callback(MachineCycleInteraction{MemoryOperation::None});
    emulator_tick_callback(MachineCycleInteraction{MemoryOperation::None});
    const bool does_half_carry_occur = (register_file.stack_pointer & 0x0f) + (unsigned_offset & 0x0f) > 0x0f;
    const bool does_carry_occur = (register_file.stack_pointer & 0xff) + (unsigned_offset & 0xff) > 0xff;
    register_file.stack_pointer += static_cast<int8_t>(unsigned_offset);
    update_flag(FLAG_ZERO_MASK, false);
    update_flag(FLAG_SUBTRACT_MASK, false);
    update_flag(FLAG_HALF_CARRY_MASK, does_half_carry_occur);
    update_flag(FLAG_CARRY_MASK, does_carry_occur);
 }

void CPU::jump_hl_0xe9() {
    register_file.program_counter = register_file.hl;
}

void CPU::load_memory_immediate16_a_0xea() {
    write_byte_and_tick(fetch_immediate16_and_tick(), register_file.a);
}

// 0xeb is an unused opcode

// 0xec is an unused opcode

// 0xed is an unused opcode

void CPU::xor_a_immediate8_0xee() {
    xor_a_uint8(fetch_immediate8_and_tick());
}

void CPU::restart_at_0x28_0xef() {
    restart_at_address(0x28);
}

void CPU::load_a_memory_high_ram_offset_immediate8_0xf0() {
    register_file.a = read_byte_and_tick(HIGH_RAM_START + fetch_immediate8_and_tick());
}

void CPU::pop_stack_af_0xf1() {
    register_file.af = pop_stack();
    register_file.flags &= 0xf0; // Lower nibble of flags must always be zeroed
}

void CPU::load_a_memory_high_ram_offset_c_0xf2() {
    register_file.a = read_byte_and_tick(HIGH_RAM_START + register_file.c);
}

void CPU::disable_interrupts_0xf3() {
    interrupt_master_enable_ime = InterruptMasterEnableState::Disabled;
}

// 0xf4 is an unused opcode

void CPU::push_stack_af_0xf5() {
    push_stack_register16(register_file.af);
}

void CPU::or_a_immediate8_0xf6() {
    or_a_uint8(fetch_immediate8_and_tick());
}

void CPU::restart_at_0x30_0xf7() {
    restart_at_address(0x30);
}

void CPU::load_hl_stack_pointer_with_signed_offset_0xf8() {
    // Carries are based on the unsigned immediate byte while the result is based on its signed equivalent
    const uint8_t unsigned_offset = fetch_immediate8_and_tick();
    emulator_tick_callback(MachineCycleInteraction{MemoryOperation::None});
    const bool does_half_carry_occur = (register_file.stack_pointer & 0x0f) + (unsigned_offset & 0x0f) > 0x0f;
    const bool does_carry_occur = (register_file.stack_pointer & 0xff) + (unsigned_offset & 0xff) > 0xff;
    register_file.hl = register_file.stack_pointer + static_cast<int8_t>(unsigned_offset);
    update_flag(FLAG_ZERO_MASK, false);
    update_flag(FLAG_SUBTRACT_MASK, false);
    update_flag(FLAG_HALF_CARRY_MASK, does_half_carry_occur);
    update_flag(FLAG_CARRY_MASK, does_carry_occur);
}

void CPU::load_stack_pointer_hl_0xf9() {
    emulator_tick_callback(MachineCycleInteraction{MemoryOperation::None});
    register_file.stack_pointer = register_file.hl;
}

void CPU::load_a_memory_immediate16_0xfa() {
    register_file.a = read_byte_and_tick(fetch_immediate16_and_tick());
}

void CPU::enable_interrupts_0xfb() {
    if (interrupt_master_enable_ime == InterruptMasterEnableState::Disabled) {
        interrupt_master_enable_ime = InterruptMasterEnableState::WillEnable;
    }
}

// 0xfc is an unused opcode

// 0xfd is an unused opcode

void CPU::compare_a_immediate8_0xfe() {
    compare_a_uint8(fetch_immediate8_and_tick());
}

void CPU::restart_at_0x38_0xff() {
    restart_at_address(0x38);
}

void CPU::rotate_left_circular_b_0xcb_0x00() {
    rotate_left_circular_uint8(register_file.b);
}

void CPU::rotate_left_circular_c_0xcb_0x01() {
    rotate_left_circular_uint8(register_file.c);
}

void CPU::rotate_left_circular_d_0xcb_0x02() {
    rotate_left_circular_uint8(register_file.d);
}

void CPU::rotate_left_circular_e_0xcb_0x03() {
    rotate_left_circular_uint8(register_file.e);
}

void CPU::rotate_left_circular_h_0xcb_0x04() {
    rotate_left_circular_uint8(register_file.h);
}

void CPU::rotate_left_circular_l_0xcb_0x05() {
    rotate_left_circular_uint8(register_file.l);
}

void CPU::rotate_left_circular_memory_hl_0xcb_0x06() {
    uint8_t memory_hl = read_byte_and_tick(register_file.hl);
    rotate_left_circular_uint8(memory_hl);
    write_byte_and_tick(register_file.hl, memory_hl);
}

void CPU::rotate_left_circular_a_0xcb_0x07() {
    rotate_left_circular_uint8(register_file.a);
}

void CPU::rotate_right_circular_b_0xcb_0x08() {
    rotate_right_circular_uint8(register_file.b);
}

void CPU::rotate_right_circular_c_0xcb_0x09() {
    rotate_right_circular_uint8(register_file.c);
}

void CPU::rotate_right_circular_d_0xcb_0x0a() {
    rotate_right_circular_uint8(register_file.d);
}

void CPU::rotate_right_circular_e_0xcb_0x0b() {
    rotate_right_circular_uint8(register_file.e);
}

void CPU::rotate_right_circular_h_0xcb_0x0c() {
    rotate_right_circular_uint8(register_file.h);
}

void CPU::rotate_right_circular_l_0xcb_0x0d() {
    rotate_right_circular_uint8(register_file.l);
}

void CPU::rotate_right_circular_memory_hl_0xcb_0x0e() {
    uint8_t memory_hl = read_byte_and_tick(register_file.hl);
    rotate_right_circular_uint8(memory_hl);
    write_byte_and_tick(register_file.hl, memory_hl);
}

void CPU::rotate_right_circular_a_0xcb_0x0f() {
    rotate_right_circular_uint8(register_file.a);
}


void CPU::rotate_left_through_carry_b_0xcb_0x10() {
    rotate_left_through_carry_uint8(register_file.b);
}

void CPU::rotate_left_through_carry_c_0xcb_0x11() {
    rotate_left_through_carry_uint8(register_file.c);
}

void CPU::rotate_left_through_carry_d_0xcb_0x12() {
    rotate_left_through_carry_uint8(register_file.d);
}

void CPU::rotate_left_through_carry_e_0xcb_0x13() {
    rotate_left_through_carry_uint8(register_file.e);
}

void CPU::rotate_left_through_carry_h_0xcb_0x14() {
    rotate_left_through_carry_uint8(register_file.h);
}

void CPU::rotate_left_through_carry_l_0xcb_0x15() {
    rotate_left_through_carry_uint8(register_file.l);
}

void CPU::rotate_left_through_carry_memory_hl_0xcb_0x16() {
    uint8_t memory_hl = read_byte_and_tick(register_file.hl);
    rotate_left_through_carry_uint8(memory_hl);
    write_byte_and_tick(register_file.hl, memory_hl);
}

void CPU::rotate_left_through_carry_a_0xcb_0x17() {
    rotate_left_through_carry_uint8(register_file.a);
}

void CPU::rotate_right_through_carry_b_0xcb_0x18() {
    rotate_right_through_carry_uint8(register_file.b);
}

void CPU::rotate_right_through_carry_c_0xcb_0x19() {
    rotate_right_through_carry_uint8(register_file.c);
}

void CPU::rotate_right_through_carry_d_0xcb_0x1a() {
    rotate_right_through_carry_uint8(register_file.d);
}

void CPU::rotate_right_through_carry_e_0xcb_0x1b() {
    rotate_right_through_carry_uint8(register_file.e);
}

void CPU::rotate_right_through_carry_h_0xcb_0x1c() {
    rotate_right_through_carry_uint8(register_file.h);
}

void CPU::rotate_right_through_carry_l_0xcb_0x1d() {
    rotate_right_through_carry_uint8(register_file.l);
}

void CPU::rotate_right_through_carry_memory_hl_0xcb_0x1e() {
    uint8_t memory_hl = read_byte_and_tick(register_file.hl);
    rotate_right_through_carry_uint8(memory_hl);
    write_byte_and_tick(register_file.hl, memory_hl);
}

void CPU::rotate_right_through_carry_a_0xcb_0x1f() {
    rotate_right_through_carry_uint8(register_file.a);
}

void CPU::shift_left_arithmetic_b_0xcb_0x20() {
    shift_left_arithmetic_uint8(register_file.b);
}

void CPU::shift_left_arithmetic_c_0xcb_0x21() {
    shift_left_arithmetic_uint8(register_file.c);
}

void CPU::shift_left_arithmetic_d_0xcb_0x22() {
    shift_left_arithmetic_uint8(register_file.d);
}

void CPU::shift_left_arithmetic_e_0xcb_0x23() {
    shift_left_arithmetic_uint8(register_file.e);
}

void CPU::shift_left_arithmetic_h_0xcb_0x24() {
    shift_left_arithmetic_uint8(register_file.h);
}

void CPU::shift_left_arithmetic_l_0xcb_0x25() {
    shift_left_arithmetic_uint8(register_file.l);
}

void CPU::shift_left_arithmetic_memory_hl_0xcb_0x26() {
    uint8_t memory_hl = read_byte_and_tick(register_file.hl);
    shift_left_arithmetic_uint8(memory_hl);
    write_byte_and_tick(register_file.hl, memory_hl);
}

void CPU::shift_left_arithmetic_a_0xcb_0x27() {
    shift_left_arithmetic_uint8(register_file.a);
}

void CPU::shift_right_arithmetic_b_0xcb_0x28() {
    shift_right_arithmetic_uint8(register_file.b);
}

void CPU::shift_right_arithmetic_c_0xcb_0x29() {
    shift_right_arithmetic_uint8(register_file.c);
}

void CPU::shift_right_arithmetic_d_0xcb_0x2a() {
    shift_right_arithmetic_uint8(register_file.d);
}

void CPU::shift_right_arithmetic_e_0xcb_0x2b() {
    shift_right_arithmetic_uint8(register_file.e);
}

void CPU::shift_right_arithmetic_h_0xcb_0x2c() {
    shift_right_arithmetic_uint8(register_file.h);
}

void CPU::shift_right_arithmetic_l_0xcb_0x2d() {
    shift_right_arithmetic_uint8(register_file.l);
}

void CPU::shift_right_arithmetic_memory_hl_0xcb_0x2e() {
    uint8_t memory_hl = read_byte_and_tick(register_file.hl);
    shift_right_arithmetic_uint8(memory_hl);
    write_byte_and_tick(register_file.hl, memory_hl);
}

void CPU::shift_right_arithmetic_a_0xcb_0x2f() {
    shift_right_arithmetic_uint8(register_file.a);
}

void CPU::swap_nibbles_b_0xcb_0x30() {
    swap_nibbles_uint8(register_file.b);
}

void CPU::swap_nibbles_c_0xcb_0x31() {
    swap_nibbles_uint8(register_file.c);
}

void CPU::swap_nibbles_d_0xcb_0x32() {
    swap_nibbles_uint8(register_file.d);
}

void CPU::swap_nibbles_e_0xcb_0x33() {
    swap_nibbles_uint8(register_file.e);
}

void CPU::swap_nibbles_h_0xcb_0x34() {
    swap_nibbles_uint8(register_file.h);
}

void CPU::swap_nibbles_l_0xcb_0x35() {
    swap_nibbles_uint8(register_file.l);
}

void CPU::swap_nibbles_memory_hl_0xcb_0x36() {
    uint8_t memory_hl = read_byte_and_tick(register_file.hl);
    swap_nibbles_uint8(memory_hl);
    write_byte_and_tick(register_file.hl, memory_hl);
}

void CPU::swap_nibbles_a_0xcb_0x37() {
    swap_nibbles_uint8(register_file.a);
}

void CPU::shift_right_logical_b_0xcb_0x38() {
    shift_right_logical_uint8(register_file.b);
}

void CPU::shift_right_logical_c_0xcb_0x39() {
    shift_right_logical_uint8(register_file.c);
}

void CPU::shift_right_logical_d_0xcb_0x3a() {
    shift_right_logical_uint8(register_file.d);
}

void CPU::shift_right_logical_e_0xcb_0x3b() {
    shift_right_logical_uint8(register_file.e);
}

void CPU::shift_right_logical_h_0xcb_0x3c() {
    shift_right_logical_uint8(register_file.h);
}

void CPU::shift_right_logical_l_0xcb_0x3d() {
    shift_right_logical_uint8(register_file.l);
}

void CPU::shift_right_logical_memory_hl_0xcb_0x3e() {
    uint8_t memory_hl = read_byte_and_tick(register_file.hl);
    shift_right_logical_uint8(memory_hl);
    write_byte_and_tick(register_file.hl, memory_hl);
}

void CPU::shift_right_logical_a_0xcb_0x3f() {
    shift_right_logical_uint8(register_file.a);
}

void CPU::test_bit_0_b_0xcb_0x40() {
    test_bit_position_uint8(0, register_file.b);
}

void CPU::test_bit_0_c_0xcb_0x41() {
    test_bit_position_uint8(0, register_file.c);
}

void CPU::test_bit_0_d_0xcb_0x42() {
    test_bit_position_uint8(0, register_file.d);
}

void CPU::test_bit_0_e_0xcb_0x43() {
    test_bit_position_uint8(0, register_file.e);
}

void CPU::test_bit_0_h_0xcb_0x44() {
    test_bit_position_uint8(0, register_file.h);
}

void CPU::test_bit_0_l_0xcb_0x45() {
    test_bit_position_uint8(0, register_file.l);
}

void CPU::test_bit_0_memory_hl_0xcb_0x46() {
    test_bit_position_memory_hl(0);
}

void CPU::test_bit_0_a_0xcb_0x47() {
    test_bit_position_uint8(0, register_file.a);
}

void CPU::test_bit_1_b_0xcb_0x48() {
    test_bit_position_uint8(1, register_file.b);
}

void CPU::test_bit_1_c_0xcb_0x49() {
    test_bit_position_uint8(1, register_file.c);
}

void CPU::test_bit_1_d_0xcb_0x4a() {
    test_bit_position_uint8(1, register_file.d);
}

void CPU::test_bit_1_e_0xcb_0x4b() {
    test_bit_position_uint8(1, register_file.e);
}

void CPU::test_bit_1_h_0xcb_0x4c() {
    test_bit_position_uint8(1, register_file.h);
}

void CPU::test_bit_1_l_0xcb_0x4d() {
    test_bit_position_uint8(1, register_file.l);
}

void CPU::test_bit_1_memory_hl_0xcb_0x4e() {
    test_bit_position_memory_hl(1);
}

void CPU::test_bit_1_a_0xcb_0x4f() {
    test_bit_position_uint8(1, register_file.a);
}

void CPU::test_bit_2_b_0xcb_0x50(){
    test_bit_position_uint8(2, register_file.b);
}

void CPU::test_bit_2_c_0xcb_0x51() {
    test_bit_position_uint8(2, register_file.c);
}

void CPU::test_bit_2_d_0xcb_0x52() {
    test_bit_position_uint8(2, register_file.d);
}

void CPU::test_bit_2_e_0xcb_0x53() {
    test_bit_position_uint8(2, register_file.e);
}

void CPU::test_bit_2_h_0xcb_0x54() {
    test_bit_position_uint8(2, register_file.h);
}

void CPU::test_bit_2_l_0xcb_0x55() {
    test_bit_position_uint8(2, register_file.l);
}

void CPU::test_bit_2_memory_hl_0xcb_0x56() {
    test_bit_position_memory_hl(2);
}

void CPU::test_bit_2_a_0xcb_0x57() {
    test_bit_position_uint8(2, register_file.a);
}

void CPU::test_bit_3_b_0xcb_0x58() {
    test_bit_position_uint8(3, register_file.b);
}

void CPU::test_bit_3_c_0xcb_0x59() {
    test_bit_position_uint8(3, register_file.c);
}

void CPU::test_bit_3_d_0xcb_0x5a() {
    test_bit_position_uint8(3, register_file.d);
}

void CPU::test_bit_3_e_0xcb_0x5b() {
    test_bit_position_uint8(3, register_file.e);
}

void CPU::test_bit_3_h_0xcb_0x5c() {
    test_bit_position_uint8(3, register_file.h);
}

void CPU::test_bit_3_l_0xcb_0x5d() {
    test_bit_position_uint8(3, register_file.l);
}

void CPU::test_bit_3_memory_hl_0xcb_0x5e() {
    test_bit_position_memory_hl(3);
}

void CPU::test_bit_3_a_0xcb_0x5f() {
    test_bit_position_uint8(3, register_file.a);
}

void CPU::test_bit_4_b_0xcb_0x60() {
    test_bit_position_uint8(4, register_file.b);
}

void CPU::test_bit_4_c_0xcb_0x61() {
    test_bit_position_uint8(4, register_file.c);
}

void CPU::test_bit_4_d_0xcb_0x62() {
    test_bit_position_uint8(4, register_file.d);
}

void CPU::test_bit_4_e_0xcb_0x63() {
    test_bit_position_uint8(4, register_file.e);
}

void CPU::test_bit_4_h_0xcb_0x64() {
    test_bit_position_uint8(4, register_file.h);
}

void CPU::test_bit_4_l_0xcb_0x65() {
    test_bit_position_uint8(4, register_file.l);
}

void CPU::test_bit_4_memory_hl_0xcb_0x66() {
    test_bit_position_memory_hl(4);
}

void CPU::test_bit_4_a_0xcb_0x67() {
    test_bit_position_uint8(4, register_file.a);
}

void CPU::test_bit_5_b_0xcb_0x68() {
    test_bit_position_uint8(5, register_file.b);
}

void CPU::test_bit_5_c_0xcb_0x69() {
    test_bit_position_uint8(5, register_file.c);
}

void CPU::test_bit_5_d_0xcb_0x6a() {
    test_bit_position_uint8(5, register_file.d);
}

void CPU::test_bit_5_e_0xcb_0x6b() {
    test_bit_position_uint8(5, register_file.e);
}

void CPU::test_bit_5_h_0xcb_0x6c() {
    test_bit_position_uint8(5, register_file.h);
}

void CPU::test_bit_5_l_0xcb_0x6d() {
    test_bit_position_uint8(5, register_file.l);
}

void CPU::test_bit_5_memory_hl_0xcb_0x6e() {
    test_bit_position_memory_hl(5);
}

void CPU::test_bit_5_a_0xcb_0x6f() {
    test_bit_position_uint8(5, register_file.a);
}

void CPU::test_bit_6_b_0xcb_0x70() {
    test_bit_position_uint8(6, register_file.b);
}

void CPU::test_bit_6_c_0xcb_0x71() {
    test_bit_position_uint8(6, register_file.c);
}

void CPU::test_bit_6_d_0xcb_0x72() {
    test_bit_position_uint8(6, register_file.d);
}

void CPU::test_bit_6_e_0xcb_0x73() {
    test_bit_position_uint8(6, register_file.e);
}

void CPU::test_bit_6_h_0xcb_0x74() {
    test_bit_position_uint8(6, register_file.h);
}

void CPU::test_bit_6_l_0xcb_0x75() {
    test_bit_position_uint8(6, register_file.l);
}

void CPU::test_bit_6_memory_hl_0xcb_0x76() {
    test_bit_position_memory_hl(6);
}

void CPU::test_bit_6_a_0xcb_0x77() {
    test_bit_position_uint8(6, register_file.a);
}

void CPU::test_bit_7_b_0xcb_0x78() {
    test_bit_position_uint8(7, register_file.b);
}

void CPU::test_bit_7_c_0xcb_0x79() {
    test_bit_position_uint8(7, register_file.c);
}

void CPU::test_bit_7_d_0xcb_0x7a() {
    test_bit_position_uint8(7, register_file.d);
}

void CPU::test_bit_7_e_0xcb_0x7b() {
    test_bit_position_uint8(7, register_file.e);
}

void CPU::test_bit_7_h_0xcb_0x7c() {
    test_bit_position_uint8(7, register_file.h);
}

void CPU::test_bit_7_l_0xcb_0x7d() {
    test_bit_position_uint8(7, register_file.l);
}

void CPU::test_bit_7_memory_hl_0xcb_0x7e() {
    test_bit_position_memory_hl(7);
}

void CPU::test_bit_7_a_0xcb_0x7f() {
    test_bit_position_uint8(7, register_file.a);
}

void CPU::reset_bit_0_b_0xcb_0x80() {
    reset_bit_position_uint8(0, register_file.b);
}

void CPU::reset_bit_0_c_0xcb_0x81() {
    reset_bit_position_uint8(0, register_file.c);
}

void CPU::reset_bit_0_d_0xcb_0x82() {
    reset_bit_position_uint8(0, register_file.d);
}

void CPU::reset_bit_0_e_0xcb_0x83() {
    reset_bit_position_uint8(0, register_file.e);
}

void CPU::reset_bit_0_h_0xcb_0x84() {
    reset_bit_position_uint8(0, register_file.h);
}

void CPU::reset_bit_0_l_0xcb_0x85() {
    reset_bit_position_uint8(0, register_file.l);
}

void CPU::reset_bit_0_memory_hl_0xcb_0x86() {
    reset_bit_position_memory_hl(0);
}

void CPU::reset_bit_0_a_0xcb_0x87() {
    reset_bit_position_uint8(0, register_file.a);
}

void CPU::reset_bit_1_b_0xcb_0x88() {
    reset_bit_position_uint8(1, register_file.b);
}

void CPU::reset_bit_1_c_0xcb_0x89() {
    reset_bit_position_uint8(1, register_file.c);
}

void CPU::reset_bit_1_d_0xcb_0x8a() {
    reset_bit_position_uint8(1, register_file.d);
}

void CPU::reset_bit_1_e_0xcb_0x8b() {
    reset_bit_position_uint8(1, register_file.e);
}

void CPU::reset_bit_1_h_0xcb_0x8c() {
    reset_bit_position_uint8(1, register_file.h);
}

void CPU::reset_bit_1_l_0xcb_0x8d() {
    reset_bit_position_uint8(1, register_file.l);
}

void CPU::reset_bit_1_memory_hl_0xcb_0x8e() {
    reset_bit_position_memory_hl(1);
}

void CPU::reset_bit_1_a_0xcb_0x8f() {
    reset_bit_position_uint8(1, register_file.a);
}

void CPU::reset_bit_2_b_0xcb_0x90() {
    reset_bit_position_uint8(2, register_file.b);
}

void CPU::reset_bit_2_c_0xcb_0x91() {
    reset_bit_position_uint8(2, register_file.c);
}

void CPU::reset_bit_2_d_0xcb_0x92() {
    reset_bit_position_uint8(2, register_file.d);
}

void CPU::reset_bit_2_e_0xcb_0x93() {
    reset_bit_position_uint8(2, register_file.e);
}

void CPU::reset_bit_2_h_0xcb_0x94() {
    reset_bit_position_uint8(2, register_file.h);
}

void CPU::reset_bit_2_l_0xcb_0x95() {
    reset_bit_position_uint8(2, register_file.l);
}

void CPU::reset_bit_2_memory_hl_0xcb_0x96() {
    reset_bit_position_memory_hl(2);
}

void CPU::reset_bit_2_a_0xcb_0x97() {
    reset_bit_position_uint8(2, register_file.a);
}

void CPU::reset_bit_3_b_0xcb_0x98() {
    reset_bit_position_uint8(3, register_file.b);
}

void CPU::reset_bit_3_c_0xcb_0x99() {
    reset_bit_position_uint8(3, register_file.c);
}

void CPU::reset_bit_3_d_0xcb_0x9a() {
    reset_bit_position_uint8(3, register_file.d);
}

void CPU::reset_bit_3_e_0xcb_0x9b() {
    reset_bit_position_uint8(3, register_file.e);
}

void CPU::reset_bit_3_h_0xcb_0x9c() {
    reset_bit_position_uint8(3, register_file.h);
}

void CPU::reset_bit_3_l_0xcb_0x9d() {
    reset_bit_position_uint8(3, register_file.l);
}

void CPU::reset_bit_3_memory_hl_0xcb_0x9e() {
    reset_bit_position_memory_hl(3);
}

void CPU::reset_bit_3_a_0xcb_0x9f() {
    reset_bit_position_uint8(3, register_file.a);
}

void CPU::reset_bit_4_b_0xcb_0xa0() {
    reset_bit_position_uint8(4, register_file.b);
}

void CPU::reset_bit_4_c_0xcb_0xa1() {
    reset_bit_position_uint8(4, register_file.c);
}

void CPU::reset_bit_4_d_0xcb_0xa2() {
    reset_bit_position_uint8(4, register_file.d);
}

void CPU::reset_bit_4_e_0xcb_0xa3() {
    reset_bit_position_uint8(4, register_file.e);
}

void CPU::reset_bit_4_h_0xcb_0xa4() {
    reset_bit_position_uint8(4, register_file.h);
}

void CPU::reset_bit_4_l_0xcb_0xa5() {
    reset_bit_position_uint8(4, register_file.l);
}

void CPU::reset_bit_4_memory_hl_0xcb_0xa6() {
    reset_bit_position_memory_hl(4);
}

void CPU::reset_bit_4_a_0xcb_0xa7() {
    reset_bit_position_uint8(4, register_file.a);
}

void CPU::reset_bit_5_b_0xcb_0xa8() {
    reset_bit_position_uint8(5, register_file.b);
}

void CPU::reset_bit_5_c_0xcb_0xa9() {
    reset_bit_position_uint8(5, register_file.c);
}

void CPU::reset_bit_5_d_0xcb_0xaa() {
    reset_bit_position_uint8(5, register_file.d);
}

void CPU::reset_bit_5_e_0xcb_0xab() {
    reset_bit_position_uint8(5, register_file.e);
}

void CPU::reset_bit_5_h_0xcb_0xac() {
    reset_bit_position_uint8(5, register_file.h);
}

void CPU::reset_bit_5_l_0xcb_0xad() {
    reset_bit_position_uint8(5, register_file.l);
}

void CPU::reset_bit_5_memory_hl_0xcb_0xae() {
    reset_bit_position_memory_hl(5);
}

void CPU::reset_bit_5_a_0xcb_0xaf() {
    reset_bit_position_uint8(5, register_file.a);
}

void CPU::reset_bit_6_b_0xcb_0xb0() {
    reset_bit_position_uint8(6, register_file.b);
}

void CPU::reset_bit_6_c_0xcb_0xb1() {
    reset_bit_position_uint8(6, register_file.c);
}

void CPU::reset_bit_6_d_0xcb_0xb2() {
    reset_bit_position_uint8(6, register_file.d);
}

void CPU::reset_bit_6_e_0xcb_0xb3() {
    reset_bit_position_uint8(6, register_file.e);
}

void CPU::reset_bit_6_h_0xcb_0xb4() {
    reset_bit_position_uint8(6, register_file.h);
}

void CPU::reset_bit_6_l_0xcb_0xb5() {
    reset_bit_position_uint8(6, register_file.l);
}

void CPU::reset_bit_6_memory_hl_0xcb_0xb6() {
    reset_bit_position_memory_hl(6);
}

void CPU::reset_bit_6_a_0xcb_0xb7() {
    reset_bit_position_uint8(6, register_file.a);
}

void CPU::reset_bit_7_b_0xcb_0xb8() {
    reset_bit_position_uint8(7, register_file.b);
}

void CPU::reset_bit_7_c_0xcb_0xb9() {
    reset_bit_position_uint8(7, register_file.c);
}

void CPU::reset_bit_7_d_0xcb_0xba() {
    reset_bit_position_uint8(7, register_file.d);
}

void CPU::reset_bit_7_e_0xcb_0xbb() {
    reset_bit_position_uint8(7, register_file.e);
}

void CPU::reset_bit_7_h_0xcb_0xbc() {
    reset_bit_position_uint8(7, register_file.h);
}

void CPU::reset_bit_7_l_0xcb_0xbd() {
    reset_bit_position_uint8(7, register_file.l);
}

void CPU::reset_bit_7_memory_hl_0xcb_0xbe() {
    reset_bit_position_memory_hl(7);
}

void CPU::reset_bit_7_a_0xcb_0xbf() {
    reset_bit_position_uint8(7, register_file.a);
}

void CPU::set_bit_0_b_0xcb_0xc0() {
    set_bit_position_uint8(0, register_file.b);
}

void CPU::set_bit_0_c_0xcb_0xc1() {
    set_bit_position_uint8(0, register_file.c);
}

void CPU::set_bit_0_d_0xcb_0xc2() {
    set_bit_position_uint8(0, register_file.d);
}

void CPU::set_bit_0_e_0xcb_0xc3() {
    set_bit_position_uint8(0, register_file.e);
}

void CPU::set_bit_0_h_0xcb_0xc4() {
    set_bit_position_uint8(0, register_file.h);
}

void CPU::set_bit_0_l_0xcb_0xc5() {
    set_bit_position_uint8(0, register_file.l);
}

void CPU::set_bit_0_memory_hl_0xcb_0xc6() {
    set_bit_position_memory_hl(0);
}

void CPU::set_bit_0_a_0xcb_0xc7() {
    set_bit_position_uint8(0, register_file.a);
}

void CPU::set_bit_1_b_0xcb_0xc8() {
    set_bit_position_uint8(1, register_file.b);
}

void CPU::set_bit_1_c_0xcb_0xc9() {
    set_bit_position_uint8(1, register_file.c);
}

void CPU::set_bit_1_d_0xcb_0xca() {
    set_bit_position_uint8(1, register_file.d);
}

void CPU::set_bit_1_e_0xcb_0xcb() {
    set_bit_position_uint8(1, register_file.e);
}

void CPU::set_bit_1_h_0xcb_0xcc() {
    set_bit_position_uint8(1, register_file.h);
}

void CPU::set_bit_1_l_0xcb_0xcd() {
    set_bit_position_uint8(1, register_file.l);
}

void CPU::set_bit_1_memory_hl_0xcb_0xce() {
    set_bit_position_memory_hl(1);
}

void CPU::set_bit_1_a_0xcb_0xcf() {
    set_bit_position_uint8(1, register_file.a);
}

void CPU::set_bit_2_b_0xcb_0xd0() {
    set_bit_position_uint8(2, register_file.b);
}

void CPU::set_bit_2_c_0xcb_0xd1() {
    set_bit_position_uint8(2, register_file.c);
}

void CPU::set_bit_2_d_0xcb_0xd2() {
    set_bit_position_uint8(2, register_file.d);
}

void CPU::set_bit_2_e_0xcb_0xd3() {
    set_bit_position_uint8(2, register_file.e);
}

void CPU::set_bit_2_h_0xcb_0xd4() {
    set_bit_position_uint8(2, register_file.h);
}

void CPU::set_bit_2_l_0xcb_0xd5() {
    set_bit_position_uint8(2, register_file.l);
}

void CPU::set_bit_2_memory_hl_0xcb_0xd6() {
    set_bit_position_memory_hl(2);
}

void CPU::set_bit_2_a_0xcb_0xd7() {
    set_bit_position_uint8(2, register_file.a);
}

void CPU::set_bit_3_b_0xcb_0xd8() {
    set_bit_position_uint8(3, register_file.b);
}

void CPU::set_bit_3_c_0xcb_0xd9() {
    set_bit_position_uint8(3, register_file.c);
}

void CPU::set_bit_3_d_0xcb_0xda() {
    set_bit_position_uint8(3, register_file.d);
}

void CPU::set_bit_3_e_0xcb_0xdb() {
    set_bit_position_uint8(3, register_file.e);
}

void CPU::set_bit_3_h_0xcb_0xdc() {
    set_bit_position_uint8(3, register_file.h);
}

void CPU::set_bit_3_l_0xcb_0xdd() {
    set_bit_position_uint8(3, register_file.l);
}

void CPU::set_bit_3_memory_hl_0xcb_0xde() {
    set_bit_position_memory_hl(3);
}

void CPU::set_bit_3_a_0xcb_0xdf() {
    set_bit_position_uint8(3, register_file.a);
}

void CPU::set_bit_4_b_0xcb_0xe0() {
    set_bit_position_uint8(4, register_file.b);
}

void CPU::set_bit_4_c_0xcb_0xe1() {
    set_bit_position_uint8(4, register_file.c);
}

void CPU::set_bit_4_d_0xcb_0xe2() {
    set_bit_position_uint8(4, register_file.d);
}

void CPU::set_bit_4_e_0xcb_0xe3() {
    set_bit_position_uint8(4, register_file.e);
}

void CPU::set_bit_4_h_0xcb_0xe4() {
    set_bit_position_uint8(4, register_file.h);
}

void CPU::set_bit_4_l_0xcb_0xe5() {
    set_bit_position_uint8(4, register_file.l);
}

void CPU::set_bit_4_memory_hl_0xcb_0xe6() {
    set_bit_position_memory_hl(4);
}

void CPU::set_bit_4_a_0xcb_0xe7() {
    set_bit_position_uint8(4, register_file.a);
}

void CPU::set_bit_5_b_0xcb_0xe8() {
    set_bit_position_uint8(5, register_file.b);
}

void CPU::set_bit_5_c_0xcb_0xe9() {
    set_bit_position_uint8(5, register_file.c);
}

void CPU::set_bit_5_d_0xcb_0xea() {
    set_bit_position_uint8(5, register_file.d);
}

void CPU::set_bit_5_e_0xcb_0xeb() {
    set_bit_position_uint8(5, register_file.e);
}

void CPU::set_bit_5_h_0xcb_0xec() {
    set_bit_position_uint8(5, register_file.h);
}

void CPU::set_bit_5_l_0xcb_0xed() {
    set_bit_position_uint8(5, register_file.l);
}

void CPU::set_bit_5_memory_hl_0xcb_0xee() {
    set_bit_position_memory_hl(5);
}

void CPU::set_bit_5_a_0xcb_0xef() {
    set_bit_position_uint8(5, register_file.a);
}

void CPU::set_bit_6_b_0xcb_0xf0() {
    set_bit_position_uint8(6, register_file.b);
}

void CPU::set_bit_6_c_0xcb_0xf1() {
    set_bit_position_uint8(6, register_file.c);
}

void CPU::set_bit_6_d_0xcb_0xf2() {
    set_bit_position_uint8(6, register_file.d);
}

void CPU::set_bit_6_e_0xcb_0xf3() {
    set_bit_position_uint8(6, register_file.e);
}

void CPU::set_bit_6_h_0xcb_0xf4() {
    set_bit_position_uint8(6, register_file.h);
}

void CPU::set_bit_6_l_0xcb_0xf5() {
    set_bit_position_uint8(6, register_file.l);
}

void CPU::set_bit_6_memory_hl_0xcb_0xf6() {
    set_bit_position_memory_hl(6);
}

void CPU::set_bit_6_a_0xcb_0xf7() {
    set_bit_position_uint8(6, register_file.a);
}

void CPU::set_bit_7_b_0xcb_0xf8() {
    set_bit_position_uint8(7, register_file.b);
}

void CPU::set_bit_7_c_0xcb_0xf9() {
    set_bit_position_uint8(7, register_file.c);
}

void CPU::set_bit_7_d_0xcb_0xfa() {
    set_bit_position_uint8(7, register_file.d);
}

void CPU::set_bit_7_e_0xcb_0xfb() {
    set_bit_position_uint8(7, register_file.e);
}

void CPU::set_bit_7_h_0xcb_0xfc() {
    set_bit_position_uint8(7, register_file.h);
}

void CPU::set_bit_7_l_0xcb_0xfd() {
    set_bit_position_uint8(7, register_file.l);
}

void CPU::set_bit_7_memory_hl_0xcb_0xfe() {
    set_bit_position_memory_hl(7);
}

void CPU::set_bit_7_a_0xcb_0xff() {
    set_bit_position_uint8(7, register_file.a);
}

} // namespace GameBoy
