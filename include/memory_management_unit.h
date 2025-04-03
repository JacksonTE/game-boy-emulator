#pragma once

#include <array>
#include <cstdint>
#include <functional>
#include "memory_interface.h"

namespace GameBoy {

class MemoryManagementUnit : public MemoryInterface {
public:
    MemoryManagementUnit();

    void reset_state() override;
    void set_post_boot_state() override;
    bool try_load_file(uint16_t address, uint32_t number_of_bytes_to_load, std::filesystem::path file_path, bool is_bootrom_file) override;

    uint8_t read_8(uint16_t address) const override;
    void write_8(uint16_t address, uint8_t value) override;

private:
    std::unique_ptr<uint8_t[]> placeholder_memory;
    std::unique_ptr<uint8_t[]> bootrom{};
};

} // namespace GameBoy
