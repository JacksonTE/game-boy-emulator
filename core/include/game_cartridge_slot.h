#include <cstdint>
#include <filesystem>
#include <fstream>
#include <memory>
#include <vector>

namespace GameBoyCore
{

constexpr uint8_t ROM_ONLY_BYTE = 0x00;
constexpr uint8_t MBC1_BYTE = 0x01;
constexpr uint8_t MBC1_WITH_RAM_BYTE = 0x02;
constexpr uint8_t MBC1_WITH_RAM_AND_BATTERY_BYTE = 0x03;
constexpr uint8_t MBC2_BYTE = 0x05;
constexpr uint8_t MBC2_WITH_BATTERY_BYTE = 0x06;

constexpr uint8_t MBC5_BYTE = 0x19;
constexpr uint8_t MBC5_WITH_RAM_BYTE = 0x1a;
constexpr uint8_t MBC5_WITH_RAM_AND_BATTERY_BYTE = 0x1b;
constexpr uint8_t MBC5_WITH_RUMBLE = 0x1c;
constexpr uint8_t MBC5_WITH_RUMBLE_AND_RAM = 0x1d;
constexpr uint8_t MBC5_WITH_RUMBLE_AND_RAM_AND_BATTERY = 0x1e;

constexpr uint16_t ROM_BANK_SIZE = 0x4000;

constexpr uint8_t LOGO_SIZE = 48;
constexpr uint16_t LOGO_START_POSITION = 0x0104;

class MemoryBankControllerBase
{
public:
    static constexpr uint16_t no_mbc_file_size_bytes = 0x8000;

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
    static constexpr uint32_t max_rom_size_in_default_configuration_bytes = 0x80000;
    static constexpr uint32_t max_rom_size_bytes = 0x200000;
    static constexpr uint32_t mbc1m_multi_game_compilation_cart_rom_size_bytes = 0x100000;

    static constexpr uint32_t max_ram_size_in_large_configuration_bytes = 0x2000;

    MBC1(std::vector<uint8_t> &rom, std::vector<uint8_t> &ram);

    uint8_t read_byte(uint16_t address) override;
    void write_byte(uint16_t address, uint8_t value) override;

private:
    bool is_ram_enabled{};
    uint8_t effective_rom_bank_number{0b00000001};
    uint8_t ram_bank_or_upper_rom_bank_number{};
    uint8_t banking_mode_select{};
};

class MBC2 : public MemoryBankControllerBase
{
public:
    static constexpr uint32_t max_rom_size_bytes = 0x40000;
    static constexpr uint8_t max_number_of_rom_banks = 0x10;
    static constexpr uint16_t built_in_ram_size_bytes = 0x200;

    MBC2(std::vector<uint8_t> &rom, std::vector<uint8_t> &ram);

    uint8_t read_byte(uint16_t address) override;
    void write_byte(uint16_t address, uint8_t value) override;

private:
    bool does_register_control_rom{};
    uint8_t ram_enable_or_rom_bank_number{};
};

class MBC5 : public MemoryBankControllerBase
{
public:
    static constexpr uint32_t max_rom_size_bytes = 0x800000;
    static constexpr uint32_t max_ram_size_bytes = 0x20000;

    MBC5(std::vector<uint8_t> &rom, std::vector<uint8_t> &ram, bool is_rumble_enabled);

    uint8_t read_byte(uint16_t address) override;
    void write_byte(uint16_t address, uint8_t value) override;

private:
    bool is_rumble_circuitry_used{};
    uint8_t bits_zero_to_seven_of_rom_bank_number{1};
    uint8_t bit_eight_of_rom_bank_number{};
    bool is_ram_enabled{};
    uint8_t ram_bank_number{};
};

class GameCartridgeSlot
{
public:
    GameCartridgeSlot();

    void reset_state();

    bool try_load_file(const std::filesystem::path &file_path, std::ifstream &file, std::streamsize file_length_in_bytes, std::string &error_message);

    uint8_t read_byte(uint16_t address) const;
    void write_byte(uint16_t address, uint8_t value);

private:
    std::vector<uint8_t> rom{};
    std::vector<uint8_t> ram{};
    std::unique_ptr<MemoryBankControllerBase> memory_bank_controller{};
};

} // namespace GameBoyCore
