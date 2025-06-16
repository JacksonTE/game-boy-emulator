#pragma once

#include <bit>
#include <cstdint>
#include <functional>

#include "memory_management_unit.h"
#include "register_file.h"

namespace GameBoyCore
{

constexpr uint8_t INSTRUCTION_PREFIX_BYTE = 0xcb;

constexpr uint16_t CARTRIDGE_HEADER_START = 0x0134;
constexpr uint16_t CARTRIDGE_HEADER_END = 0x014c;

enum class InterruptMasterEnableState
{
    Disabled,
    WillEnable,
    Enabled
};

class CentralProcessingUnit
{
public:
    CentralProcessingUnit(std::function<void()> emulator_step_single_machine_cycle, MemoryManagementUnit &memory_management_unit_reference);

    void reset_state(bool should_add_startup_machine_cycle);
    void set_post_boot_state();

    RegisterFile<std::endian::native> get_register_file() const;
    void set_register_file_state(const RegisterFile<std::endian::native> &new_register_values);

    void step_single_instruction();

private:
    std::function<void()> emulator_step_single_machine_cycle_callback;
    MemoryManagementUnit &memory_management_unit;
    RegisterFile<std::endian::native> register_file;
    InterruptMasterEnableState interrupt_master_enable_ime{InterruptMasterEnableState::Disabled};
    uint8_t instruction_register_ir{};
    bool is_current_instruction_prefixed{};
    bool is_halted{};

    void fetch_next_instruction();
    void service_interrupt();

    virtual uint8_t read_byte_and_step_emulator_components(uint16_t address);
    virtual void write_byte_and_step_emulator_components(uint16_t address, uint8_t value);
    uint8_t fetch_immediate8_and_step_emulator_components();
    uint16_t fetch_immediate16_and_step_emulator_components();

    uint8_t &get_register_by_index(uint8_t index);
    void decode_current_unprefixed_opcode_and_execute();
    void decode_current_prefixed_opcode_and_execute();

    // Generic Instructions
    template <typename T>
    void load(T &destination_register, T value);
    void load_memory(uint16_t address, uint8_t value);
    void increment(uint8_t &register8);
    void increment_and_step_emulator_components(uint16_t &register16);
    void decrement(uint8_t &register8);
    void decrement_and_step_emulator_components(uint16_t &register16);
    void add_hl(uint16_t value);
    void add_a(uint8_t value);
    void add_with_carry_a(uint8_t value);
    void subtract_a(uint8_t value);
    void subtract_with_carry_a(uint8_t value);
    void and_a(uint8_t value);
    void xor_a(uint8_t value);
    void or_a(uint8_t value);
    void compare_a(uint8_t value);
    void jump_relative_conditional_signed_immediate8(bool is_condition_met);
    void jump_conditional_immediate16(bool is_condition_met);
    void pop_stack(uint16_t &destination_register16);
    void push_stack(uint16_t value);
    void call_conditional_immediate16(bool is_condition_met);
    void return_conditional(bool is_condition_met);
    void restart_at_address(uint16_t address);

    void rotate_left_circular(uint8_t &register8);
    void rotate_right_circular(uint8_t &register8);
    void rotate_left_through_carry(uint8_t &register8);
    void rotate_right_through_carry(uint8_t &register8);
    void shift_left_arithmetic(uint8_t &register8);
    void shift_right_arithmetic(uint8_t &register8);
    void swap_nibbles(uint8_t &register8);
    void shift_right_logical(uint8_t &register8);
    void test_bit(uint8_t bit_position_to_test, uint8_t &register8);
    void reset_bit(uint8_t bit_position_to_reset, uint8_t &register8);
    void set_bit(uint8_t bit_position_to_set, uint8_t &register8);

    void operate_on_register_hl(void (CentralProcessingUnit:: *operation)(uint8_t, uint8_t &), uint8_t bit_position);
    void operate_on_register_hl_and_write(void (CentralProcessingUnit:: *operation)(uint8_t, uint8_t &), uint8_t bit_position);
    void operate_on_register_hl_and_write(void (CentralProcessingUnit:: *operation)(uint8_t &));

    // Miscellaneous Instructions
    void unused_opcode() const;
    void rotate_left_circular_a_0x07();
    void load_memory_immediate16_stack_pointer_0x08();
    void rotate_right_circular_a_0x0f();
    void rotate_left_through_carry_a_0x17();
    void rotate_right_through_carry_a_0x1f();
    void decimal_adjust_a_0x27();
    void complement_a_0x2f();
    void set_carry_flag_0x37();
    void complement_carry_flag_0x3f();
    void halt_0x76();
    void return_0xc9();
    void return_from_interrupt_0xd9();
    void add_stack_pointer_signed_immediate8_0xe8();
    void jump_hl_0xe9();
    void pop_stack_af_0xf1();
    void disable_interrupts_0xf3();
    void push_stack_af_0xf5();
    void load_hl_stack_pointer_with_signed_offset_0xf8();
    void load_stack_pointer_hl_0xf9();
    void enable_interrupts_0xfb();
};

} // namespace GameBoyCore
