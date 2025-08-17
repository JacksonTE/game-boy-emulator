#pragma once

#include <cstdint>
#include <vector>

namespace GameBoyCore
{

constexpr uint8_t ROM_ONLY_BYTE = 0x00;
constexpr uint8_t MBC1_BYTE = 0x01;
constexpr uint8_t MBC1_WITH_RAM_BYTE = 0x02;
constexpr uint8_t MBC1_WITH_RAM_AND_BATTERY_BYTE = 0x03;

constexpr uint8_t MBC2_BYTE = 0x05;
constexpr uint8_t MBC2_WITH_BATTERY_BYTE = 0x06;

constexpr uint8_t MBC3_WITH_TIMER_AND_BATTERY_BYTE = 0x0F;
constexpr uint8_t MBC3_WITH_TIMER_AND_RAM_AND_BATTERY_BYTE = 0x10;
constexpr uint8_t MBC3_BYTE = 0x11;
constexpr uint8_t MBC3_WITH_RAM_BYTE = 0x12;
constexpr uint8_t MBC3_WITH_RAM_AND_BATTERY_BYTE = 0x13;

constexpr uint8_t MBC5_BYTE = 0x19;
constexpr uint8_t MBC5_WITH_RAM_BYTE = 0x1A;
constexpr uint8_t MBC5_WITH_RAM_AND_BATTERY_BYTE = 0x1B;
constexpr uint8_t MBC5_WITH_RUMBLE = 0x1C;
constexpr uint8_t MBC5_WITH_RUMBLE_AND_RAM = 0x1D;
constexpr uint8_t MBC5_WITH_RUMBLE_AND_RAM_AND_BATTERY = 0x1E;

constexpr uint16_t ROM_BANK_SIZE = 0x4000;
constexpr uint8_t ROM_BANK_SIZE_POWER_OF_TWO = 14;
constexpr uint16_t RAM_BANK_SIZE = 0x2000;
constexpr uint8_t RAM_BANK_SIZE_POWER_OF_TWO = 13;

class MemoryBankControllerBase
{
public:
    static constexpr uint16_t ROM_ONLY_WITH_NO_MBC_FILE_SIZE = 0x8000;

    MemoryBankControllerBase(std::vector<uint8_t> &rom, std::vector<uint8_t> &ram);

    virtual uint8_t read_byte(uint16_t address);
    virtual void write_byte(uint16_t address, uint8_t value);

protected:
    const std::vector<uint8_t> &cartridge_rom;
    std::vector<uint8_t> &cartridge_ram;
};

class MBC1 : public MemoryBankControllerBase
{
public:
    static constexpr int MINIMUM_ALLOWABLE_ROM_BANK_NUMBER = 1;
    static constexpr uint32_t MAX_ROM_SIZE_IN_DEFAULT_CONFIGURATION = 0x80000;
    static constexpr uint32_t MAX_ROM_SIZE = 0x200000;
    static constexpr uint32_t MBC1M_MULTI_GAME_COMPILATION_CART_ROM_SIZE = 0x100000;

    static constexpr uint32_t MAX_RAM_SIZE_IN_LARGE_CONFIGURATION = 0x2000;

    MBC1(std::vector<uint8_t> &rom, std::vector<uint8_t> &ram);

    uint8_t read_byte(uint16_t address) override;
    void write_byte(uint16_t address, uint8_t value) override;

private:
    uint8_t number_of_rom_banks;
    uint8_t number_of_ram_banks;

    bool is_ram_enabled{};
    uint8_t lower_five_bits_of_rom_bank_number{MINIMUM_ALLOWABLE_ROM_BANK_NUMBER};
    uint8_t ram_bank_number_or_upper_two_bits_of_rom_bank_number{};
    uint8_t banking_mode{};
};

class MBC2 : public MemoryBankControllerBase
{
public:
    static constexpr int MINIMUM_ALLOWABLE_ROM_BANK_NUMBER = 1;
    static constexpr uint32_t MAX_ROM_SIZE = 0x40000;
    static constexpr uint32_t MAX_NUMBER_OF_ROM_BANKS = 0x10;
    static constexpr uint16_t BUILT_IN_RAM_SIZE = 0x200;

    MBC2(std::vector<uint8_t> &rom, std::vector<uint8_t> &ram);

    uint8_t read_byte(uint16_t address) override;
    void write_byte(uint16_t address, uint8_t value) override;

private:
    bool is_ram_enabled{};
    uint8_t selected_rom_bank_number{MINIMUM_ALLOWABLE_ROM_BANK_NUMBER};
};

class MBC3 : public MemoryBankControllerBase
{
public:
    static constexpr int MINIMUM_ALLOWABLE_ROM_BANK_NUMBER = 1;
    static constexpr uint32_t MAX_ROM_SIZE = 0x200000;
    static constexpr uint32_t MAX_RAM_SIZE = 0x8000;

    MBC3(std::vector<uint8_t> &rom, std::vector<uint8_t> &ram);

    uint8_t read_byte(uint16_t address) override;
    void write_byte(uint16_t address, uint8_t value) override;

private:
    uint8_t number_of_rom_banks;
    uint8_t number_of_ram_banks;

    bool are_ram_and_real_time_clock_enabled{};
    uint8_t selected_rom_bank_number{MINIMUM_ALLOWABLE_ROM_BANK_NUMBER};
    uint8_t selected_ram_bank_number_or_real_time_clock_register_select{};

    struct RealTimeClock
    {
        bool is_halted{};
        bool is_day_counter_carry_set{};
        bool are_time_counters_latched{};
        uint8_t latch_clock_data{};
        uint8_t seconds_counter{};
        uint8_t minutes_counter{};
        uint8_t hours_counter{};
        uint16_t days_counter{};
    } real_time_clock;
};

class MBC5 : public MemoryBankControllerBase
{
public:
    static constexpr uint32_t MAX_ROM_SIZE = 0x800000;
    static constexpr uint32_t MAX_RAM_SIZE = 0x20000;

    MBC5(std::vector<uint8_t> &rom, std::vector<uint8_t> &ram);

    uint8_t read_byte(uint16_t address) override;
    void write_byte(uint16_t address, uint8_t value) override;

private:
    uint8_t number_of_rom_banks;
    uint8_t number_of_ram_banks;

    bool is_ram_enabled{};
    uint8_t selected_ram_bank_number{};
    uint16_t selected_rom_bank_number{1};
};

} // namespace GameBoyCore
