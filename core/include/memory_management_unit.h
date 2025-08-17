#pragma once

#include <atomic>
#include <cstdint>
#include <filesystem>

#include "game_cartridge_slot.h"
#include "internal_timer.h"
#include "pixel_processing_unit.h"

namespace GameBoyCore
{

constexpr uint32_t MEMORY_SIZE = 0x10000;
constexpr uint16_t BOOTROM_SIZE = 0x100;

constexpr uint16_t EXTERNAL_RAM_SIZE = 0x2000;
constexpr uint16_t WORK_RAM_SIZE = 0x2000;
constexpr uint16_t ECHO_RAM_SIZE = 0x1e00;
constexpr uint16_t UNUSABLE_MEMORY_SIZE = 0x0060;
constexpr uint16_t INPUT_OUTPUT_REGISTERS_SIZE = 0x0080;
constexpr uint16_t HIGH_RAM_SIZE = 0x007f;

constexpr uint16_t ROM_BANK_X0_START = 0x0000;
constexpr uint16_t ROM_BANK_0X_START = 0x4000;
constexpr uint16_t EXTERNAL_RAM_START = 0xa000;
constexpr uint16_t WORK_RAM_START = 0xc000;
constexpr uint16_t ECHO_RAM_START = 0xe000;
constexpr uint16_t UNUSABLE_MEMORY_START = 0xfea0;
constexpr uint16_t INPUT_OUTPUT_REGISTERS_START = 0xff00;
constexpr uint16_t HIGH_RAM_START = 0xff80;

constexpr uint8_t OAM_DMA_MACHINE_CYCLE_DURATION = 0xa0;

constexpr uint8_t NUMBER_OF_INTERRUPT_TYPES = 5;
constexpr uint8_t JOYPAD_INTERRUPT_FLAG_MASK = 1 << 4;
constexpr uint8_t SERIAL_INTERRUPT_FLAG_MASK = 1 << 3;
constexpr uint8_t TIMER_INTERRUPT_FLAG_MASK = 1 << 2;

constexpr uint8_t RIGHT_DPAD_DIRECTION_FLAG_MASK = 1 << 0;
constexpr uint8_t LEFT_DPAD_DIRECTION_FLAG_MASK = 1 << 1;
constexpr uint8_t UP_DPAD_DIRECTION_FLAG_MASK = 1 << 2;
constexpr uint8_t DOWN_DPAD_DIRECTION_FLAG_MASK = 1 << 3;

constexpr uint8_t A_BUTTON_FLAG_MASK = 1 << 0;
constexpr uint8_t B_BUTTON_FLAG_MASK = 1 << 1;
constexpr uint8_t SELECT_BUTTON_FLAG_MASK = 1 << 2;
constexpr uint8_t START_BUTTON_FLAG_MASK = 1 << 3;

enum class FileType
{
    GameROM,
    BootROM
};

enum class ObjectAttributeMemoryDirectMemoryAccessStartupState
{
    NotStarting,
    RegisterWrittenTo,
    Starting
};

class MemoryManagementUnit
{
public:
    MemoryManagementUnit(GameCartridgeSlot &game_cartridge_slot_reference, InternalTimer &internal_timer_reference, PixelProcessingUnit &pixel_processing_unit_reference);

    virtual void reset_state();
    void set_post_boot_state();
    bool try_load_file(const std::filesystem::path &file_path, FileType file_type, std::string &error_message);
    void unload_bootrom_thread_safe();
    void unload_game_rom_thread_safe();
    bool is_game_rom_loaded_thread_safe() const;
    bool is_bootrom_loaded_thread_safe() const;
    bool is_bootrom_mapped() const;

    virtual uint8_t read_byte(uint16_t address, bool is_access_unrestricted) const;
    virtual void write_byte(uint16_t address, uint8_t value, bool is_access_unrestricted);

    void step_single_machine_cycle();

    void request_interrupt(uint8_t interrupt_flag_mask);
    void clear_interrupt_flag_bit(uint8_t interrupt_flag_mask);
    uint8_t get_pending_interrupt_mask() const;

    void update_button_pressed_state_thread_safe(uint8_t button_flag_mask, bool is_button_pressed);
    void update_dpad_direction_pressed_state_thread_safe(uint8_t direction_flag_mask, bool is_direction_pressed);

private:
    std::unique_ptr<uint8_t[]> bootrom{};
    std::unique_ptr<uint8_t[]> work_ram{};
    std::unique_ptr<uint8_t[]> unmapped_input_output_registers{};
    std::unique_ptr<uint8_t[]> high_ram{};

    GameCartridgeSlot &game_cartridge_slot;
    InternalTimer &internal_timer;
    PixelProcessingUnit &pixel_processing_unit;

    std::atomic<bool> atomic_is_bootrom_loaded_in_memory{};
    std::atomic<bool> atomic_is_game_rom_loaded_in_memory{};

    std::atomic<uint8_t> atomic_button_pressed_states{0b11111111};
    std::atomic<uint8_t> atomic_dpad_direction_pressed_states{0b11111111};
    std::atomic<uint8_t> atomic_most_recent_currently_pressed_vertical_direction{0b11111111};
    std::atomic<uint8_t> atomic_most_recent_currently_pressed_horizontal_direction{0b11111111};
    uint8_t joypad_p1_joyp{0b11111111};
    uint8_t interrupt_flag_if{0b11100000};
    uint8_t bootrom_status{};
    uint8_t interrupt_enable_ie{};

    ObjectAttributeMemoryDirectMemoryAccessStartupState oam_dma_startup_state{ObjectAttributeMemoryDirectMemoryAccessStartupState::NotStarting};
    uint16_t oam_dma_source_address_base{};
    uint8_t oam_dma_machine_cycles_elapsed{};

    bool are_addresses_on_same_bus(uint16_t first_address, uint16_t second_address) const;
};

} // namespace GameBoyCore
