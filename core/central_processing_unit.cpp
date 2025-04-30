#include <bit>
#include <iomanip>
#include <iostream>

#include "central_processing_unit.h"
#include "bitwise_utilities.h"

namespace GameBoy
{

MachineCycleOperation::MachineCycleOperation(MemoryInteraction interaction)
    : memory_interaction{interaction}
{
}

MachineCycleOperation::MachineCycleOperation(MemoryInteraction interaction, uint16_t address)
    : memory_interaction{interaction},
      address_accessed{address}
{
}

MachineCycleOperation::MachineCycleOperation(MemoryInteraction interaction, uint16_t address, uint8_t value)
    : memory_interaction{interaction},
      address_accessed{address},
      value_written{value}
{
}

bool MachineCycleOperation::operator==(const MachineCycleOperation &other) const
{
    if (memory_interaction != other.memory_interaction)
        return false;

    switch (memory_interaction)
    {
        case MemoryInteraction::None:
            return true;
        case MemoryInteraction::Read:
            return address_accessed == other.address_accessed;
        case MemoryInteraction::Write:
            return address_accessed == other.address_accessed && value_written == other.value_written;
        default:
            return false;
    }
}

CentralProcessingUnit::CentralProcessingUnit(std::function<void(MachineCycleOperation)> emulator_step_single_machine_cycle, MemoryManagementUnit &memory_management_unit_reference)
    : emulator_step_single_machine_cycle_callback{emulator_step_single_machine_cycle},
      memory_management_unit{memory_management_unit_reference}
{
}

void CentralProcessingUnit::reset_state()
{
    set_register_file_state(RegisterFile<std::endian::native>{});
    interrupt_master_enable_ime = InterruptMasterEnableState::Disabled;
    instruction_register_ir = 0x00;
    is_current_instruction_prefixed = false;
    is_halted = false;
}

void CentralProcessingUnit::set_post_boot_state()
{
    reset_state();
    register_file.a = 0x01;
    update_flag(register_file.flags, FLAG_ZERO_MASK, true);
    update_flag(register_file.flags, FLAG_SUBTRACT_MASK, false);
    
    uint8_t header_checksum = 0;
    for (uint16_t address = CARTRIDGE_HEADER_START; address <= CARTRIDGE_HEADER_END; address++)
    {
        header_checksum -= memory_management_unit.read_byte(BOOTROM_SIZE + address, false) - 1;
    }
    update_flag(register_file.flags, FLAG_HALF_CARRY_MASK, header_checksum != 0);
    update_flag(register_file.flags, FLAG_CARRY_MASK, header_checksum != 0);
    register_file.b = 0x00;
    register_file.c = 0x13;
    register_file.d = 0x00;
    register_file.e = 0xd8;
    register_file.h = 0x01;
    register_file.l = 0x4d;
    register_file.program_counter = 0x100;
    register_file.stack_pointer = 0xfffe;
}

RegisterFile<std::endian::native> CentralProcessingUnit::get_register_file() const
{
    return register_file;
}

void CentralProcessingUnit::set_register_file_state(const RegisterFile<std::endian::native> &new_register_values)
{
    register_file.a = new_register_values.a;
    register_file.flags = new_register_values.flags & 0xf0; // Lower nibble of flags must always be zeroed
    register_file.bc = new_register_values.bc;
    register_file.de = new_register_values.de;
    register_file.hl = new_register_values.hl;
    register_file.program_counter = new_register_values.program_counter;
    register_file.stack_pointer = new_register_values.stack_pointer;
}

void CentralProcessingUnit::step_single_instruction()
{
    if (is_halted)
        emulator_step_single_machine_cycle_callback(MachineCycleOperation{MemoryInteraction::None});
    else
    {
        if (!is_current_instruction_prefixed)
            decode_current_unprefixed_opcode_and_execute();
        else
            decode_current_prefixed_opcode_and_execute();

        fetch_next_instruction();
    }
    service_interrupt();

    if (interrupt_master_enable_ime == InterruptMasterEnableState::WillEnable)
        interrupt_master_enable_ime = InterruptMasterEnableState::Enabled;
}

void CentralProcessingUnit::fetch_next_instruction()
{
    const uint8_t immediate8 = fetch_immediate8_and_step_emulator_components();
    is_current_instruction_prefixed = (immediate8 == INSTRUCTION_PREFIX_BYTE);
    instruction_register_ir = is_current_instruction_prefixed
        ? fetch_immediate8_and_step_emulator_components()
        : immediate8;
}

void CentralProcessingUnit::service_interrupt()
{
    bool is_interrupt_pending = (memory_management_unit.get_pending_interrupt_mask() != 0);
    if (is_interrupt_pending && is_halted)
        is_halted = false;

    if (interrupt_master_enable_ime != InterruptMasterEnableState::Enabled || !is_interrupt_pending)
        return;

    decrement_and_step_emulator_components(register_file.program_counter);
    decrement_and_step_emulator_components(register_file.stack_pointer);
    write_byte_and_step_emulator_components(register_file.stack_pointer--, register_file.program_counter >> 8);
    uint8_t interrupt_flag_mask = memory_management_unit.get_pending_interrupt_mask();
    write_byte_and_step_emulator_components(register_file.stack_pointer, register_file.program_counter & 0xff);

    memory_management_unit.clear_interrupt_flag_bit(interrupt_flag_mask);
    interrupt_master_enable_ime = InterruptMasterEnableState::Disabled;
    register_file.program_counter = (interrupt_flag_mask == 0x00)
        ? 0x00
        : 0x0040 + 8 * static_cast<uint8_t>(std::countr_zero(interrupt_flag_mask));

    fetch_next_instruction();
}

uint8_t CentralProcessingUnit::read_byte_and_step_emulator_components(uint16_t address)
{
    emulator_step_single_machine_cycle_callback(MachineCycleOperation{MemoryInteraction::Read, address});
    return memory_management_unit.read_byte(address, false);
}

void CentralProcessingUnit::write_byte_and_step_emulator_components(uint16_t address, uint8_t value)
{
    emulator_step_single_machine_cycle_callback(MachineCycleOperation{MemoryInteraction::Write, address, value});
    memory_management_unit.write_byte(address, value, false);
}

uint8_t CentralProcessingUnit::fetch_immediate8_and_step_emulator_components()
{
    uint8_t immediate8 = read_byte_and_step_emulator_components(register_file.program_counter);
    if (!is_halted)
    {
        register_file.program_counter++;
    }
    return immediate8;
}

uint16_t CentralProcessingUnit::fetch_immediate16_and_step_emulator_components()
{
    const uint8_t low_byte = fetch_immediate8_and_step_emulator_components();
    return low_byte | static_cast<uint16_t>(fetch_immediate8_and_step_emulator_components() << 8);
}

uint8_t &CentralProcessingUnit::get_register_by_index(uint8_t index)
{
    switch (index)
    {
        case 0: return register_file.b;
        case 1: return register_file.c;
        case 2: return register_file.d;
        case 3: return register_file.e;
        case 4: return register_file.h;
        case 5: return register_file.l;
        case 7: return register_file.a;
    }
}

void CentralProcessingUnit::decode_current_unprefixed_opcode_and_execute()
{
    const uint8_t destination_register_index = ((instruction_register_ir >> 3) & 0b111);
    const uint8_t source_register_index = (instruction_register_ir & 0b111);

    switch (instruction_register_ir)
    {
        case 0x00:
            // no operation instruction - NOP
            break;
        case 0x01:
            load(register_file.bc, fetch_immediate16_and_step_emulator_components());
            break;
        case 0x02:
            load_memory(register_file.bc, register_file.a);
            break;
        case 0x03:
            increment_and_step_emulator_components(register_file.bc);
            break;
        case 0x04:
            increment(register_file.b);
            break;
        case 0x05:
            decrement(register_file.b);
            break;
        case 0x06:
            load(register_file.b, fetch_immediate8_and_step_emulator_components());
            break;
        case 0x07:
            rotate_left_circular_a_0x07();
            break;
        case 0x08:
            load_memory_immediate16_stack_pointer_0x08();
            break;
        case 0x09:
            add_hl(register_file.bc);
            break;
        case 0x0a:
            load(register_file.a, read_byte_and_step_emulator_components(register_file.bc));
            break;
        case 0x0b:
            decrement_and_step_emulator_components(register_file.bc);
            break;
        case 0x0c:
            increment(register_file.c);
            break;
        case 0x0d:
            decrement(register_file.c);
            break;
        case 0x0e:
            load(register_file.c, fetch_immediate8_and_step_emulator_components());
            break;
        case 0x0f:
            rotate_right_circular_a_0x0f();
            break;
        case 0x10:
            // stop instruction - STOP - unused until Game Boy Color
            break;
        case 0x11:
            load(register_file.de, fetch_immediate16_and_step_emulator_components());
            break;
        case 0x12:
            load_memory(register_file.de, register_file.a);
            break;
        case 0x13:
            increment_and_step_emulator_components(register_file.de);
            break;
        case 0x14:
            increment(register_file.d);
            break;
        case 0x15:
            decrement(register_file.d);
            break;
        case 0x16:
            load(register_file.d, fetch_immediate8_and_step_emulator_components());
            break;
        case 0x17:
            rotate_left_through_carry_a_0x17();
            break;
        case 0x18:
            jump_relative_conditional_signed_immediate8(true);
            break;
        case 0x19:
            add_hl(register_file.de);
            break;
        case 0x1a:
            load(register_file.a, read_byte_and_step_emulator_components(register_file.de));
            break;
        case 0x1b:
            decrement_and_step_emulator_components(register_file.de);
            break;
        case 0x1c:
            increment(register_file.e);
            break;
        case 0x1d:
            decrement(register_file.e);
            break;
        case 0x1e:
            load(register_file.e, fetch_immediate8_and_step_emulator_components());
            break;
        case 0x1f:
            rotate_right_through_carry_a_0x1f();
            break;
        case 0x20:
            jump_relative_conditional_signed_immediate8(!is_flag_set(register_file.flags, FLAG_ZERO_MASK));
            break;
        case 0x21:
            load(register_file.hl, fetch_immediate16_and_step_emulator_components());
            break;
        case 0x22:
            load_memory(register_file.hl++, register_file.a);
            break;
        case 0x23:
            increment_and_step_emulator_components(register_file.hl);
            break;
        case 0x24:
            increment(register_file.h);
            break;
        case 0x25:
            decrement(register_file.h);
            break;
        case 0x26:
            load(register_file.h, fetch_immediate8_and_step_emulator_components());
            break;
        case 0x27:
            decimal_adjust_a_0x27();
            break;
        case 0x28:
            jump_relative_conditional_signed_immediate8(is_flag_set(register_file.flags, FLAG_ZERO_MASK));
            break;
        case 0x29:
            add_hl(register_file.hl);
            break;
        case 0x2a:
            load(register_file.a, read_byte_and_step_emulator_components(register_file.hl++));
            break;
        case 0x2b:
            decrement_and_step_emulator_components(register_file.hl);
            break;
        case 0x2c:
            increment(register_file.l);
            break;
        case 0x2d:
            decrement(register_file.l);
            break;
        case 0x2e:
            load(register_file.l, fetch_immediate8_and_step_emulator_components());
            break;
        case 0x2f:
            complement_a_0x2f();
            break;
        case 0x30:
            jump_relative_conditional_signed_immediate8(!is_flag_set(register_file.flags, FLAG_CARRY_MASK));
            break;
        case 0x31:
            load(register_file.stack_pointer, fetch_immediate16_and_step_emulator_components());
            break;
        case 0x32:
            load_memory(register_file.hl--, register_file.a);
            break;
        case 0x33:
            increment_and_step_emulator_components(register_file.stack_pointer);
            break;
        case 0x34:
            operate_on_register_hl_and_write(&CentralProcessingUnit::increment);
            break;
        case 0x35:
            operate_on_register_hl_and_write(&CentralProcessingUnit::decrement);
            break;
        case 0x36:
            load_memory(register_file.hl, fetch_immediate8_and_step_emulator_components());
            break;
        case 0x37:
            set_carry_flag_0x37();
            break;
        case 0x38:
            jump_relative_conditional_signed_immediate8(is_flag_set(register_file.flags, FLAG_CARRY_MASK));
            break;
        case 0x39:
            add_hl(register_file.stack_pointer);
            break;
        case 0x3a:
            load(register_file.a, read_byte_and_step_emulator_components(register_file.hl--));
            break;
        case 0x3b:
            decrement_and_step_emulator_components(register_file.stack_pointer);
            break;
        case 0x3c:
            increment(register_file.a);
            break;
        case 0x3d:
            decrement(register_file.a);
            break;
        case 0x3e:
            load(register_file.a, fetch_immediate8_and_step_emulator_components());
            break;
        case 0x3f:
            complement_carry_flag_0x3f();
            break;
        case 0x40:
        case 0x41:
        case 0x42:
        case 0x43:
        case 0x44:
        case 0x45:
        case 0x47:
        case 0x48:
        case 0x49:
        case 0x4a:
        case 0x4b:
        case 0x4c:
        case 0x4d:
        case 0x4f:
        case 0x50:
        case 0x51:
        case 0x52:
        case 0x53:
        case 0x54:
        case 0x55:
        case 0x57:
        case 0x58:
        case 0x59:
        case 0x5a:
        case 0x5b:
        case 0x5c:
        case 0x5d:
        case 0x5f:
        case 0x60:
        case 0x61:
        case 0x62:
        case 0x63:
        case 0x64:
        case 0x65:
        case 0x67:
        case 0x68:
        case 0x69:
        case 0x6a:
        case 0x6b:
        case 0x6c:
        case 0x6d:
        case 0x6f:
        case 0x78:
        case 0x79:
        case 0x7a:
        case 0x7b:
        case 0x7c:
        case 0x7d:
        case 0x7f:
            load(get_register_by_index(destination_register_index), get_register_by_index(source_register_index));
            break;
        case 0x46:
        case 0x56:
        case 0x66:
        case 0x4e:
        case 0x5e:
        case 0x6e:
        case 0x7e:
            load(get_register_by_index(destination_register_index), read_byte_and_step_emulator_components(register_file.hl));
            break;
        case 0x70:
        case 0x71:
        case 0x72:
        case 0x73:
        case 0x74:
        case 0x75:
        case 0x77:
            load_memory(register_file.hl, get_register_by_index(source_register_index));
            break;
        case 0x76:
            halt_0x76();
            break;
        case 0x80:
        case 0x81:
        case 0x82:
        case 0x83:
        case 0x84:
        case 0x85:
        case 0x87:
            add_a(get_register_by_index(source_register_index));
            break;
        case 0x86:
            add_a(read_byte_and_step_emulator_components(register_file.hl));
            break;
        case 0x88:
        case 0x89:
        case 0x8a:
        case 0x8b:
        case 0x8c:
        case 0x8d:
        case 0x8f:
            add_with_carry_a(get_register_by_index(source_register_index));
            break;
        case 0x8e:
            add_with_carry_a(read_byte_and_step_emulator_components(register_file.hl));
            break;
        case 0x90:
        case 0x91:
        case 0x92:
        case 0x93:
        case 0x94:
        case 0x95:
        case 0x97:
            subtract_a(get_register_by_index(source_register_index));
            break;
        case 0x96:
            subtract_a(read_byte_and_step_emulator_components(register_file.hl));
            break;
        case 0x98:
        case 0x99:
        case 0x9a:
        case 0x9b:
        case 0x9c:
        case 0x9d:
        case 0x9f:
            subtract_with_carry_a(get_register_by_index(source_register_index));
            break;
        case 0x9e:
            subtract_with_carry_a(read_byte_and_step_emulator_components(register_file.hl));
            break;
        case 0xa0:
        case 0xa1:
        case 0xa2:
        case 0xa3:
        case 0xa4:
        case 0xa5:
        case 0xa7:
            and_a(get_register_by_index(source_register_index));
            break;
        case 0xa6:
            and_a(read_byte_and_step_emulator_components(register_file.hl));
            break;
        case 0xa8:
        case 0xa9:
        case 0xaa:
        case 0xab:
        case 0xac:
        case 0xad:
        case 0xaf:
            xor_a(get_register_by_index(source_register_index));
            break;
        case 0xae:
            xor_a(read_byte_and_step_emulator_components(register_file.hl));
            break;
        case 0xb0:
        case 0xb1:
        case 0xb2:
        case 0xb3:
        case 0xb4:
        case 0xb5:
        case 0xb7:
            or_a(get_register_by_index(source_register_index));
            break;
        case 0xb6:
            or_a(read_byte_and_step_emulator_components(register_file.hl));
            break;
        case 0xb8:
        case 0xb9:
        case 0xba:
        case 0xbb:
        case 0xbc:
        case 0xbd:
        case 0xbf:
            compare_a(get_register_by_index(source_register_index));
            break;
        case 0xbe:
            compare_a(read_byte_and_step_emulator_components(register_file.hl));
            break;
        case 0xc0:
            return_conditional(!is_flag_set(register_file.flags, FLAG_ZERO_MASK));
            break;
        case 0xc1:
            pop_stack(register_file.bc);
            break;
        case 0xc2:
            jump_conditional_immediate16(!is_flag_set(register_file.flags, FLAG_ZERO_MASK));
            break;
        case 0xc3:
            jump_conditional_immediate16(true);
            break;
        case 0xc4:
            call_conditional_immediate16(!is_flag_set(register_file.flags, FLAG_ZERO_MASK));
            break;
        case 0xc5:
            push_stack(register_file.bc);
            break;
        case 0xc6:
            add_a(fetch_immediate8_and_step_emulator_components());
            break;
        case 0xc7:
            restart_at_address(0x00);
            break;
        case 0xc8:
            return_conditional(is_flag_set(register_file.flags, FLAG_ZERO_MASK));
            break;
        case 0xc9:
            return_0xc9();
            break;
        case 0xca:
            jump_conditional_immediate16(is_flag_set(register_file.flags, FLAG_ZERO_MASK));
            break;
        case 0xcc:
            call_conditional_immediate16(is_flag_set(register_file.flags, FLAG_ZERO_MASK));
            break;
        case 0xcd:
            call_conditional_immediate16(true);
            break;
        case 0xce:
            add_with_carry_a(fetch_immediate8_and_step_emulator_components());
            break;
        case 0xcf:
            restart_at_address(0x08);
            break;
        case 0xd0:
            return_conditional(!is_flag_set(register_file.flags, FLAG_CARRY_MASK));
            break;
        case 0xd1:
            pop_stack(register_file.de);
            break;
        case 0xd2:
            jump_conditional_immediate16(!is_flag_set(register_file.flags, FLAG_CARRY_MASK));
            break;
        case 0xd4:
            call_conditional_immediate16(!is_flag_set(register_file.flags, FLAG_CARRY_MASK));
            break;
        case 0xd5:
            push_stack(register_file.de);
            break;
        case 0xd6:
            subtract_a(fetch_immediate8_and_step_emulator_components());
            break;
        case 0xd7:
            restart_at_address(0x10);
            break;
        case 0xd8:
            return_conditional(is_flag_set(register_file.flags, FLAG_CARRY_MASK));
            break;
        case 0xd9:
            return_from_interrupt_0xd9();
            break;
        case 0xda:
            jump_conditional_immediate16(is_flag_set(register_file.flags, FLAG_CARRY_MASK));
            break;
        case 0xdc:
            call_conditional_immediate16(is_flag_set(register_file.flags, FLAG_CARRY_MASK));
            break;
        case 0xde:
            subtract_with_carry_a(fetch_immediate8_and_step_emulator_components());
            break;
        case 0xdf:
            restart_at_address(0x18);
            break;
        case 0xe0:
            load_memory(INPUT_OUTPUT_REGISTERS_START + fetch_immediate8_and_step_emulator_components(), register_file.a);
            break;
        case 0xe1:
            pop_stack(register_file.hl);
            break;
        case 0xe2:
            load_memory(INPUT_OUTPUT_REGISTERS_START + register_file.c, register_file.a);
            break;
        case 0xe5:
            push_stack(register_file.hl);
            break;
        case 0xe6:
            and_a(fetch_immediate8_and_step_emulator_components());
            break;
        case 0xe7:
            restart_at_address(0x20);
            break;
        case 0xe8:
            add_stack_pointer_signed_immediate8_0xe8();
            break;
        case 0xe9:
            jump_hl_0xe9();
            break;
        case 0xea:
            load_memory(fetch_immediate16_and_step_emulator_components(), register_file.a);
            break;
        case 0xee:
            xor_a(fetch_immediate8_and_step_emulator_components());
            break;
        case 0xef:
            restart_at_address(0x28);
            break;
        case 0xf0:
            load(register_file.a, read_byte_and_step_emulator_components(INPUT_OUTPUT_REGISTERS_START + fetch_immediate8_and_step_emulator_components()));
            break;
        case 0xf1:
            pop_stack_af_0xf1();
            break;
        case 0xf2:
            load(register_file.a, read_byte_and_step_emulator_components(INPUT_OUTPUT_REGISTERS_START + register_file.c));
            break;
        case 0xf3:
            disable_interrupts_0xf3();
            break;
        case 0xf5:
            push_stack(register_file.af);
            break;
        case 0xf6:
            or_a(fetch_immediate8_and_step_emulator_components());
            break;
        case 0xf7:
            restart_at_address(0x30);
            break;
        case 0xf8:
            load_hl_stack_pointer_with_signed_offset_0xf8();
            break;
        case 0xf9:
            load_stack_pointer_hl_0xf9();
            break;
        case 0xfa:
            load(register_file.a, read_byte_and_step_emulator_components(fetch_immediate16_and_step_emulator_components()));
            break;
        case 0xfb:
            enable_interrupts_0xfb();
            break;
        case 0xfe:
            compare_a(fetch_immediate8_and_step_emulator_components());
            break;
        case 0xff:
            restart_at_address(0x38);
            break;
        default:
            unused_opcode();
            break;
    }
}

void CentralProcessingUnit::decode_current_prefixed_opcode_and_execute()
{
    const uint8_t destination_register_index = (instruction_register_ir & 0b111);
    const uint8_t bit_position = ((instruction_register_ir >> 3) & 0b111);

    switch (instruction_register_ir)
    {
        case 0x00:
        case 0x01:
        case 0x02:
        case 0x03:
        case 0x04:
        case 0x05:
        case 0x07:
            rotate_left_circular(get_register_by_index(destination_register_index));
            break;
        case 0x06:
            operate_on_register_hl_and_write(&CentralProcessingUnit::rotate_left_circular);
            break;
        case 0x08:
        case 0x09:
        case 0x0a:
        case 0x0b:
        case 0x0c:
        case 0x0d:
        case 0x0f:
            rotate_right_circular(get_register_by_index(destination_register_index));
            break;
        case 0x0e:
            operate_on_register_hl_and_write(&CentralProcessingUnit::rotate_right_circular);
            break;
        case 0x10:
        case 0x11:
        case 0x12:
        case 0x13:
        case 0x14:
        case 0x15:
        case 0x17:
            rotate_left_through_carry(get_register_by_index(destination_register_index));
            break;
        case 0x16:
            operate_on_register_hl_and_write(&CentralProcessingUnit::rotate_left_through_carry);
            break;
        case 0x18:
        case 0x19:
        case 0x1a:
        case 0x1b:
        case 0x1c:
        case 0x1d:
        case 0x1f:
            rotate_right_through_carry(get_register_by_index(destination_register_index));
            break;
        case 0x1e:
            operate_on_register_hl_and_write(&CentralProcessingUnit::rotate_right_through_carry);
            break;
        case 0x20:
        case 0x21:
        case 0x22:
        case 0x23:
        case 0x24:
        case 0x25:
        case 0x27:
            shift_left_arithmetic(get_register_by_index(destination_register_index));
            break;
        case 0x26:
            operate_on_register_hl_and_write(&CentralProcessingUnit::shift_left_arithmetic);
            break;
        case 0x28:
        case 0x29:
        case 0x2a:
        case 0x2b:
        case 0x2c:
        case 0x2d:
        case 0x2f:
            shift_right_arithmetic(get_register_by_index(destination_register_index));
            break;
        case 0x2e:
            operate_on_register_hl_and_write(&CentralProcessingUnit::shift_right_arithmetic);
            break;
        case 0x30:
        case 0x31:
        case 0x32:
        case 0x33:
        case 0x34:
        case 0x35:
        case 0x37:
            swap_nibbles(get_register_by_index(destination_register_index));
            break;
        case 0x36:
            operate_on_register_hl_and_write(&CentralProcessingUnit::swap_nibbles);
            break;
        case 0x38:
        case 0x39:
        case 0x3a:
        case 0x3b:
        case 0x3c:
        case 0x3d:
        case 0x3f:
            shift_right_logical(get_register_by_index(destination_register_index));
            break;
        case 0x3e:
            operate_on_register_hl_and_write(&CentralProcessingUnit::shift_right_logical);
            break;        
        case 0x40:
        case 0x41:
        case 0x42:
        case 0x43:
        case 0x44:
        case 0x45:
        case 0x47:
        case 0x48:
        case 0x49:
        case 0x4a:
        case 0x4b:
        case 0x4c:
        case 0x4d:
        case 0x4f:
        case 0x50:
        case 0x51:
        case 0x52:
        case 0x53:
        case 0x54:
        case 0x55:
        case 0x57:
        case 0x58:
        case 0x59:
        case 0x5a:
        case 0x5b:
        case 0x5c:
        case 0x5d:
        case 0x5f:
        case 0x60:
        case 0x61:
        case 0x62:
        case 0x63:
        case 0x64:
        case 0x65:
        case 0x67:
        case 0x68:
        case 0x69:
        case 0x6a:
        case 0x6b:
        case 0x6c:
        case 0x6d:
        case 0x6f:
        case 0x70:
        case 0x71:
        case 0x72:
        case 0x73:
        case 0x74:
        case 0x75:
        case 0x77:
        case 0x78:
        case 0x79:
        case 0x7a:
        case 0x7b:
        case 0x7c:
        case 0x7d:
        case 0x7f:
            test_bit(bit_position, get_register_by_index(destination_register_index));
            break;
        case 0x46:
        case 0x4e:
        case 0x56:
        case 0x5e:
        case 0x66:
        case 0x6e:
        case 0x76:
        case 0x7e:
            operate_on_register_hl(&CentralProcessingUnit::test_bit, bit_position);
            break;
        case 0x80:
        case 0x81:
        case 0x82:
        case 0x83:
        case 0x84:
        case 0x85:
        case 0x87:
        case 0x88:
        case 0x89:
        case 0x8a:
        case 0x8b:
        case 0x8c:
        case 0x8d:
        case 0x8f:
        case 0x90:
        case 0x91:
        case 0x92:
        case 0x93:
        case 0x94:
        case 0x95:
        case 0x97:
        case 0x98:
        case 0x99:
        case 0x9a:
        case 0x9b:
        case 0x9c:
        case 0x9d:
        case 0x9f:
        case 0xa0:
        case 0xa1:
        case 0xa2:
        case 0xa3:
        case 0xa4:
        case 0xa5:
        case 0xa7:
        case 0xa8:
        case 0xa9:
        case 0xaa:
        case 0xab:
        case 0xac:
        case 0xad:
        case 0xaf:
        case 0xb0:
        case 0xb1:
        case 0xb2:
        case 0xb3:
        case 0xb4:
        case 0xb5:
        case 0xb7:
        case 0xb8:
        case 0xb9:
        case 0xba:
        case 0xbb:
        case 0xbc:
        case 0xbd:
        case 0xbf:
            reset_bit(bit_position, get_register_by_index(destination_register_index));
            break;
        case 0x86:
        case 0x8e:
        case 0x96:
        case 0x9e:
        case 0xa6:
        case 0xae:
        case 0xb6:
        case 0xbe:
            operate_on_register_hl_and_write(&CentralProcessingUnit::reset_bit, bit_position);
            break;
        case 0xc0:
        case 0xc1:
        case 0xc2:
        case 0xc3:
        case 0xc4:
        case 0xc5:
        case 0xc7:
        case 0xc8:
        case 0xc9:
        case 0xca:
        case 0xcb:
        case 0xcc:
        case 0xcd:
        case 0xcf:
        case 0xd0:
        case 0xd1:
        case 0xd2:
        case 0xd3:
        case 0xd4:
        case 0xd5:
        case 0xd7:
        case 0xd8:
        case 0xd9:
        case 0xda:
        case 0xdb:
        case 0xdc:
        case 0xdd:
        case 0xdf:
        case 0xe0:
        case 0xe1:
        case 0xe2:
        case 0xe3:
        case 0xe4:
        case 0xe5:
        case 0xe7:
        case 0xe8:
        case 0xe9:
        case 0xea:
        case 0xeb:
        case 0xec:
        case 0xed:
        case 0xef:
        case 0xf0:
        case 0xf1:
        case 0xf2:
        case 0xf3:
        case 0xf4:
        case 0xf5:
        case 0xf7:
        case 0xf8:
        case 0xf9:
        case 0xfa:
        case 0xfb:
        case 0xfc:
        case 0xfd:
        case 0xff:
            set_bit(bit_position, get_register_by_index(destination_register_index));
            break;
        case 0xc6:
        case 0xce:
        case 0xd6:
        case 0xde:
        case 0xe6:
        case 0xee:
        case 0xf6:
        case 0xfe:
            operate_on_register_hl_and_write(&CentralProcessingUnit::set_bit, bit_position);
            break;
    }
}

// ================================
// ===== Generic Instructions =====
// ================================

template <typename T>
void CentralProcessingUnit::load(T &destination_register, T value)
{
    destination_register = value;
}

void CentralProcessingUnit::load_memory(uint16_t address, uint8_t value)
{
    write_byte_and_step_emulator_components(address, value);
}

void CentralProcessingUnit::increment(uint8_t &register8)
{
    const bool does_half_carry_occur = (register8 & 0x0f) == 0x0f;
    register8++;
    update_flag(register_file.flags, FLAG_ZERO_MASK, register8 == 0);
    update_flag(register_file.flags, FLAG_SUBTRACT_MASK, false);
    update_flag(register_file.flags, FLAG_HALF_CARRY_MASK, does_half_carry_occur);
}

void CentralProcessingUnit::increment_and_step_emulator_components(uint16_t &register16)
{
    emulator_step_single_machine_cycle_callback(MachineCycleOperation{MemoryInteraction::None});
    register16++;
}

void CentralProcessingUnit::decrement(uint8_t &register8)
{
    const bool does_half_carry_occur = (register8 & 0x0f) == 0x00;
    register8--;
    update_flag(register_file.flags, FLAG_ZERO_MASK, register8 == 0);
    update_flag(register_file.flags, FLAG_SUBTRACT_MASK, true);
    update_flag(register_file.flags, FLAG_HALF_CARRY_MASK, does_half_carry_occur);
}

void CentralProcessingUnit::decrement_and_step_emulator_components(uint16_t &register16)
{
    emulator_step_single_machine_cycle_callback(MachineCycleOperation{MemoryInteraction::None});
    register16--;
}

void CentralProcessingUnit::add_hl(uint16_t value)
{
    emulator_step_single_machine_cycle_callback(MachineCycleOperation{MemoryInteraction::None});
    const bool does_half_carry_occur = (register_file.hl & 0x0fff) + (value & 0x0fff) > 0x0fff;
    const bool does_carry_occur = static_cast<uint32_t>(register_file.hl) + value > 0xffff;
    register_file.hl += value;
    update_flag(register_file.flags, FLAG_SUBTRACT_MASK, false);
    update_flag(register_file.flags, FLAG_HALF_CARRY_MASK, does_half_carry_occur);
    update_flag(register_file.flags, FLAG_CARRY_MASK, does_carry_occur);
}

void CentralProcessingUnit::add_a(uint8_t value)
{
    const bool does_half_carry_occur = (register_file.a & 0x0f) + (value & 0x0f) > 0x0f;
    const bool does_carry_occur = static_cast<uint16_t>(register_file.a) + value > 0xff;
    register_file.a += value;
    update_flag(register_file.flags, FLAG_ZERO_MASK, register_file.a == 0);
    update_flag(register_file.flags, FLAG_SUBTRACT_MASK, false);
    update_flag(register_file.flags, FLAG_HALF_CARRY_MASK, does_half_carry_occur);
    update_flag(register_file.flags, FLAG_CARRY_MASK, does_carry_occur);
}

void CentralProcessingUnit::add_with_carry_a(uint8_t value)
{
    const uint8_t carry_in = is_flag_set(register_file.flags, FLAG_CARRY_MASK) ? 1 : 0;
    const bool does_half_carry_occur = (register_file.a & 0x0f) + (value & 0x0f) + carry_in > 0x0f;
    const bool does_carry_occur = static_cast<uint16_t>(register_file.a) + value + carry_in > 0xff;
    register_file.a += value + carry_in;
    update_flag(register_file.flags, FLAG_ZERO_MASK, register_file.a == 0);
    update_flag(register_file.flags, FLAG_SUBTRACT_MASK, false);
    update_flag(register_file.flags, FLAG_HALF_CARRY_MASK, does_half_carry_occur);
    update_flag(register_file.flags, FLAG_CARRY_MASK, does_carry_occur);
}

void CentralProcessingUnit::subtract_a(uint8_t value)
{
    const bool does_half_carry_occur = (register_file.a & 0x0f) < (value & 0x0f);
    const bool does_carry_occur = register_file.a < value;
    register_file.a -= value;
    update_flag(register_file.flags, FLAG_ZERO_MASK, register_file.a == 0);
    update_flag(register_file.flags, FLAG_SUBTRACT_MASK, true);
    update_flag(register_file.flags, FLAG_HALF_CARRY_MASK, does_half_carry_occur);
    update_flag(register_file.flags, FLAG_CARRY_MASK, does_carry_occur);
}

void CentralProcessingUnit::subtract_with_carry_a(uint8_t value)
{
    const uint8_t carry_in = is_flag_set(register_file.flags, FLAG_CARRY_MASK) ? 1 : 0;
    const bool does_half_carry_occur = (register_file.a & 0x0f) < (value & 0x0f) + carry_in;
    const bool does_carry_occur = register_file.a < value + carry_in;
    register_file.a -= value + carry_in;
    update_flag(register_file.flags, FLAG_ZERO_MASK, register_file.a == 0);
    update_flag(register_file.flags, FLAG_SUBTRACT_MASK, true);
    update_flag(register_file.flags, FLAG_HALF_CARRY_MASK, does_half_carry_occur);
    update_flag(register_file.flags, FLAG_CARRY_MASK, does_carry_occur);
}

void CentralProcessingUnit::and_a(uint8_t value)
{
    register_file.a &= value;
    update_flag(register_file.flags, FLAG_ZERO_MASK, register_file.a == 0);
    update_flag(register_file.flags, FLAG_SUBTRACT_MASK, false);
    update_flag(register_file.flags, FLAG_HALF_CARRY_MASK, true);
    update_flag(register_file.flags, FLAG_CARRY_MASK, false);
}

void CentralProcessingUnit::xor_a(uint8_t value)
{
    register_file.a ^= value;
    update_flag(register_file.flags, FLAG_ZERO_MASK, register_file.a == 0);
    update_flag(register_file.flags, FLAG_SUBTRACT_MASK, false);
    update_flag(register_file.flags, FLAG_HALF_CARRY_MASK, false);
    update_flag(register_file.flags, FLAG_CARRY_MASK, false);
}

void CentralProcessingUnit::or_a(uint8_t value)
{
    register_file.a |= value;
    update_flag(register_file.flags, FLAG_ZERO_MASK, register_file.a == 0);
    update_flag(register_file.flags, FLAG_SUBTRACT_MASK, false);
    update_flag(register_file.flags, FLAG_HALF_CARRY_MASK, false);
    update_flag(register_file.flags, FLAG_CARRY_MASK, false);
}

void CentralProcessingUnit::compare_a(uint8_t value)
{
    const bool does_half_carry_occur = (register_file.a & 0x0f) < (value & 0x0f);
    const bool does_carry_occur = register_file.a < value;
    update_flag(register_file.flags, FLAG_ZERO_MASK, register_file.a == value);
    update_flag(register_file.flags, FLAG_SUBTRACT_MASK, true);
    update_flag(register_file.flags, FLAG_HALF_CARRY_MASK, does_half_carry_occur);
    update_flag(register_file.flags, FLAG_CARRY_MASK, does_carry_occur);
}

void CentralProcessingUnit::jump_relative_conditional_signed_immediate8(bool is_condition_met)
{
    const int8_t signed_offset = static_cast<int8_t>(fetch_immediate8_and_step_emulator_components());
    if (is_condition_met)
    {
        emulator_step_single_machine_cycle_callback(MachineCycleOperation{MemoryInteraction::None});
        register_file.program_counter += signed_offset;
    }
}

void CentralProcessingUnit::jump_conditional_immediate16(bool is_condition_met)
{
    const uint16_t jump_address = fetch_immediate16_and_step_emulator_components();
    if (is_condition_met)
    {
        emulator_step_single_machine_cycle_callback(MachineCycleOperation{MemoryInteraction::None});
        register_file.program_counter = jump_address;
    }
}

void CentralProcessingUnit::pop_stack(uint16_t &destination_register16)
{
    uint8_t low_byte = read_byte_and_step_emulator_components(register_file.stack_pointer++);
    destination_register16 = low_byte | static_cast<uint16_t>(read_byte_and_step_emulator_components(register_file.stack_pointer++) << 8);
}

void CentralProcessingUnit::push_stack(uint16_t value)
{
    decrement_and_step_emulator_components(register_file.stack_pointer);
    write_byte_and_step_emulator_components(register_file.stack_pointer--, value >> 8);
    write_byte_and_step_emulator_components(register_file.stack_pointer, value & 0xff);
}

void CentralProcessingUnit::call_conditional_immediate16(bool is_condition_met)
{
    const uint16_t subroutine_address = fetch_immediate16_and_step_emulator_components();
    if (is_condition_met)
        restart_at_address(subroutine_address);
}

void CentralProcessingUnit::return_conditional(bool is_condition_met)
{
    emulator_step_single_machine_cycle_callback(MachineCycleOperation{MemoryInteraction::None});
    if (is_condition_met)
        return_0xc9();
}

void CentralProcessingUnit::restart_at_address(uint16_t address)
{
    push_stack(register_file.program_counter);
    register_file.program_counter = address;
}

void CentralProcessingUnit::rotate_left_circular(uint8_t &register8)
{
    const bool does_carry_occur = (register8 & 0b10000000) != 0;
    register8 = (register8 << 1) | (register8 >> 7);
    update_flag(register_file.flags, FLAG_ZERO_MASK, register8 == 0);
    update_flag(register_file.flags, FLAG_SUBTRACT_MASK, false);
    update_flag(register_file.flags, FLAG_HALF_CARRY_MASK, false);
    update_flag(register_file.flags, FLAG_CARRY_MASK, does_carry_occur);
}

void CentralProcessingUnit::rotate_right_circular(uint8_t &register8)
{
    const bool does_carry_occur = (register8 & 0b00000001) != 0;
    register8 = (register8 << 7) | (register8 >> 1);
    update_flag(register_file.flags, FLAG_ZERO_MASK, register8 == 0);
    update_flag(register_file.flags, FLAG_SUBTRACT_MASK, false);
    update_flag(register_file.flags, FLAG_HALF_CARRY_MASK, false);
    update_flag(register_file.flags, FLAG_CARRY_MASK, does_carry_occur);
}

void CentralProcessingUnit::rotate_left_through_carry(uint8_t &register8)
{
    const uint8_t carry_in = is_flag_set(register_file.flags, FLAG_CARRY_MASK) ? 1 : 0;
    const bool does_carry_occur = (register8 & 0b10000000) != 0;
    register8 = (register8 << 1) | carry_in;
    update_flag(register_file.flags, FLAG_ZERO_MASK, register8 == 0);
    update_flag(register_file.flags, FLAG_SUBTRACT_MASK, false);
    update_flag(register_file.flags, FLAG_HALF_CARRY_MASK, false);
    update_flag(register_file.flags, FLAG_CARRY_MASK, does_carry_occur);
}

void CentralProcessingUnit::rotate_right_through_carry(uint8_t &register8)
{
    const uint8_t carry_in = is_flag_set(register_file.flags, FLAG_CARRY_MASK) ? 1 : 0;
    const bool does_carry_occur = (register8 & 0b00000001) != 0;
    register8 = (carry_in << 7) | (register8 >> 1);
    update_flag(register_file.flags, FLAG_ZERO_MASK, register8 == 0);
    update_flag(register_file.flags, FLAG_SUBTRACT_MASK, false);
    update_flag(register_file.flags, FLAG_HALF_CARRY_MASK, false);
    update_flag(register_file.flags, FLAG_CARRY_MASK, does_carry_occur);
}

void CentralProcessingUnit::shift_left_arithmetic(uint8_t &register8)
{
    const bool does_carry_occur = (register8 & 0b10000000) != 0;
    register8 <<= 1;
    update_flag(register_file.flags, FLAG_ZERO_MASK, register8 == 0);
    update_flag(register_file.flags, FLAG_SUBTRACT_MASK, false);
    update_flag(register_file.flags, FLAG_HALF_CARRY_MASK, false);
    update_flag(register_file.flags, FLAG_CARRY_MASK, does_carry_occur);
}

void CentralProcessingUnit::shift_right_arithmetic(uint8_t &register8)
{
    const bool does_carry_occur = (register8 & 0b00000001) != 0;
    const uint8_t preserved_sign_bit = register8 & 0b10000000;
    register8 = preserved_sign_bit | (register8 >> 1);
    update_flag(register_file.flags, FLAG_ZERO_MASK, register8 == 0);
    update_flag(register_file.flags, FLAG_SUBTRACT_MASK, false);
    update_flag(register_file.flags, FLAG_HALF_CARRY_MASK, false);
    update_flag(register_file.flags, FLAG_CARRY_MASK, does_carry_occur);
}

void CentralProcessingUnit::swap_nibbles(uint8_t &register8)
{
    register8 = ((register8 & 0x0f) << 4) | ((register8 & 0xf0) >> 4);
    update_flag(register_file.flags, FLAG_ZERO_MASK, register8 == 0);
    update_flag(register_file.flags, FLAG_SUBTRACT_MASK, false);
    update_flag(register_file.flags, FLAG_HALF_CARRY_MASK, false);
    update_flag(register_file.flags, FLAG_CARRY_MASK, false);
}

void CentralProcessingUnit::shift_right_logical(uint8_t &register8)
{
    const bool does_carry_occur = (register8 & 0b00000001) != 0;
    register8 >>= 1;
    update_flag(register_file.flags, FLAG_ZERO_MASK, register8 == 0);
    update_flag(register_file.flags, FLAG_SUBTRACT_MASK, false);
    update_flag(register_file.flags, FLAG_HALF_CARRY_MASK, false);
    update_flag(register_file.flags, FLAG_CARRY_MASK, does_carry_occur);
}

void CentralProcessingUnit::test_bit(uint8_t bit_position_to_test, uint8_t &register8)
{
    const bool is_bit_set = (register8 & (1 << bit_position_to_test)) != 0;
    update_flag(register_file.flags, FLAG_ZERO_MASK, !is_bit_set);
    update_flag(register_file.flags, FLAG_SUBTRACT_MASK, false);
    update_flag(register_file.flags, FLAG_HALF_CARRY_MASK, true);
}

void CentralProcessingUnit::reset_bit(uint8_t bit_position_to_test, uint8_t &register8)
{
    register8 &= ~(1 << bit_position_to_test);
}

void CentralProcessingUnit::set_bit(uint8_t bit_position_to_test, uint8_t &register8)
{
    register8 |= (1 << bit_position_to_test);
}

void CentralProcessingUnit::operate_on_register_hl(void (CentralProcessingUnit:: *operation)(uint8_t, uint8_t &), uint8_t bit_position)
{
    uint8_t memory_hl = read_byte_and_step_emulator_components(register_file.hl);
    (this->*operation)(bit_position, memory_hl);
}

void CentralProcessingUnit::operate_on_register_hl_and_write(void (CentralProcessingUnit:: *operation)(uint8_t, uint8_t &), uint8_t bit_position)
{
    uint8_t memory_hl = read_byte_and_step_emulator_components(register_file.hl);
    (this->*operation)(bit_position, memory_hl);
    write_byte_and_step_emulator_components(register_file.hl, memory_hl);
}

void CentralProcessingUnit::operate_on_register_hl_and_write(void (CentralProcessingUnit:: *operation)(uint8_t &))
{
    uint8_t memory_hl = read_byte_and_step_emulator_components(register_file.hl);
    (this->*operation)(memory_hl);
    write_byte_and_step_emulator_components(register_file.hl, memory_hl);
}

// ======================================
// ===== Miscellaneous Instructions =====
// ======================================

void CentralProcessingUnit::unused_opcode() const
{
    std::cerr << std::hex << std::setfill('0');
    std::cerr << "Warning: Unused opcode 0x" << std::setw(2) 
              << static_cast<int>(instruction_register_ir) << " "
              << "encountered at memory address 0x" << std::setw(4) 
              << static_cast<int>(register_file.program_counter - 1) << "\n";
}

void CentralProcessingUnit::rotate_left_circular_a_0x07()
{
    rotate_left_circular(register_file.a);
    update_flag(register_file.flags, FLAG_ZERO_MASK, false);
}

void CentralProcessingUnit::load_memory_immediate16_stack_pointer_0x08()
{
    uint16_t immediate16 = fetch_immediate16_and_step_emulator_components();
    const uint8_t stack_pointer_low_byte = static_cast<uint8_t>(register_file.stack_pointer & 0xff);
    const uint8_t stack_pointer_high_byte = static_cast<uint8_t>(register_file.stack_pointer >> 8);
    write_byte_and_step_emulator_components(immediate16, stack_pointer_low_byte);
    write_byte_and_step_emulator_components(immediate16 + 1, stack_pointer_high_byte);
}

void CentralProcessingUnit::rotate_right_circular_a_0x0f()
{
    rotate_right_circular(register_file.a);
    update_flag(register_file.flags, FLAG_ZERO_MASK, false);
}

void CentralProcessingUnit::rotate_left_through_carry_a_0x17()
{
    rotate_left_through_carry(register_file.a);
    update_flag(register_file.flags, FLAG_ZERO_MASK, false);
}

void CentralProcessingUnit::rotate_right_through_carry_a_0x1f()
{
    rotate_right_through_carry(register_file.a);
    update_flag(register_file.flags, FLAG_ZERO_MASK, false);
}

void CentralProcessingUnit::decimal_adjust_a_0x27()
{
    const bool was_addition_most_recent = !is_flag_set(register_file.flags, FLAG_SUBTRACT_MASK);
    bool does_carry_occur = false;
    uint8_t adjustment = 0;
    // Previous operation was between two binary coded decimals (BCDs) and this corrects register A back to BCD format
    if (is_flag_set(register_file.flags, FLAG_HALF_CARRY_MASK) || (was_addition_most_recent && (register_file.a & 0x0f) > 0x09))
    {
        adjustment |= 0x06;
    }
    if (is_flag_set(register_file.flags, FLAG_CARRY_MASK) || (was_addition_most_recent && register_file.a > 0x99))
    {
        adjustment |= 0x60;
        does_carry_occur = true;
    }
    register_file.a = was_addition_most_recent
        ? (register_file.a + adjustment)
        : (register_file.a - adjustment);
    update_flag(register_file.flags, FLAG_ZERO_MASK, register_file.a == 0);
    update_flag(register_file.flags, FLAG_HALF_CARRY_MASK, false);
    update_flag(register_file.flags, FLAG_CARRY_MASK, does_carry_occur);
}

void CentralProcessingUnit::complement_a_0x2f()
{
    register_file.a = ~register_file.a;
    update_flag(register_file.flags, FLAG_SUBTRACT_MASK, true);
    update_flag(register_file.flags, FLAG_HALF_CARRY_MASK, true);
}

void CentralProcessingUnit::set_carry_flag_0x37()
{
    update_flag(register_file.flags, FLAG_SUBTRACT_MASK, false);
    update_flag(register_file.flags, FLAG_HALF_CARRY_MASK, false);
    update_flag(register_file.flags, FLAG_CARRY_MASK, true);
}

void CentralProcessingUnit::complement_carry_flag_0x3f()
{
    update_flag(register_file.flags, FLAG_SUBTRACT_MASK, false);
    update_flag(register_file.flags, FLAG_HALF_CARRY_MASK, false);
    update_flag(register_file.flags, FLAG_CARRY_MASK, !is_flag_set(register_file.flags, FLAG_CARRY_MASK));
}

void CentralProcessingUnit::halt_0x76()
{
    is_halted = true;
}

void CentralProcessingUnit::return_0xc9()
{
    uint16_t stack_top = 0;
    pop_stack(stack_top);
    emulator_step_single_machine_cycle_callback(MachineCycleOperation{MemoryInteraction::None});
    register_file.program_counter = stack_top;
}

void CentralProcessingUnit::return_from_interrupt_0xd9()
{
    interrupt_master_enable_ime = InterruptMasterEnableState::WillEnable;
    return_0xc9();
}

void CentralProcessingUnit::add_stack_pointer_signed_immediate8_0xe8()
{
    // Carries are based on the unsigned immediate byte while the result is based on its signed equivalent
    const uint8_t unsigned_offset = fetch_immediate8_and_step_emulator_components();
    emulator_step_single_machine_cycle_callback(MachineCycleOperation{MemoryInteraction::None});
    emulator_step_single_machine_cycle_callback(MachineCycleOperation{MemoryInteraction::None});
    const bool does_half_carry_occur = (register_file.stack_pointer & 0x0f) + (unsigned_offset & 0x0f) > 0x0f;
    const bool does_carry_occur = (register_file.stack_pointer & 0xff) + (unsigned_offset & 0xff) > 0xff;
    register_file.stack_pointer += static_cast<int8_t>(unsigned_offset);
    update_flag(register_file.flags, FLAG_ZERO_MASK, false);
    update_flag(register_file.flags, FLAG_SUBTRACT_MASK, false);
    update_flag(register_file.flags, FLAG_HALF_CARRY_MASK, does_half_carry_occur);
    update_flag(register_file.flags, FLAG_CARRY_MASK, does_carry_occur);
 }

void CentralProcessingUnit::jump_hl_0xe9()
{
    register_file.program_counter = register_file.hl;
}

void CentralProcessingUnit::pop_stack_af_0xf1()
{
    pop_stack(register_file.af);
    register_file.flags &= 0xf0; // Lower nibble of flags must always be zeroed
}

void CentralProcessingUnit::disable_interrupts_0xf3()
{
    interrupt_master_enable_ime = InterruptMasterEnableState::Disabled;
}

void CentralProcessingUnit::load_hl_stack_pointer_with_signed_offset_0xf8()
{
    // Carries are based on the unsigned immediate byte while the result is based on its signed equivalent
    const uint8_t unsigned_offset = fetch_immediate8_and_step_emulator_components();
    emulator_step_single_machine_cycle_callback(MachineCycleOperation{MemoryInteraction::None});
    const bool does_half_carry_occur = (register_file.stack_pointer & 0x0f) + (unsigned_offset & 0x0f) > 0x0f;
    const bool does_carry_occur = (register_file.stack_pointer & 0xff) + (unsigned_offset & 0xff) > 0xff;
    register_file.hl = register_file.stack_pointer + static_cast<int8_t>(unsigned_offset);
    update_flag(register_file.flags, FLAG_ZERO_MASK, false);
    update_flag(register_file.flags, FLAG_SUBTRACT_MASK, false);
    update_flag(register_file.flags, FLAG_HALF_CARRY_MASK, does_half_carry_occur);
    update_flag(register_file.flags, FLAG_CARRY_MASK, does_carry_occur);
}

void CentralProcessingUnit::load_stack_pointer_hl_0xf9()
{
    emulator_step_single_machine_cycle_callback(MachineCycleOperation{MemoryInteraction::None});
    register_file.stack_pointer = register_file.hl;
}

void CentralProcessingUnit::enable_interrupts_0xfb()
{
    if (interrupt_master_enable_ime == InterruptMasterEnableState::Disabled)
        interrupt_master_enable_ime = InterruptMasterEnableState::WillEnable;
}

} // namespace GameBoy
