#pragma once

#include <cstdint>
#include <ostream>
#include <string>

namespace GameBoy {

enum class MemoryOperation {
    None,
    Read,
    Write
};

std::string memory_operation_to_string(MemoryOperation op);

struct MachineCycleInteraction {
    MemoryOperation memory_operation;
    uint16_t address_accessed{};
    uint8_t value_written{};

    MachineCycleInteraction(MemoryOperation operation);
    MachineCycleInteraction(MemoryOperation operation, uint16_t address);
    MachineCycleInteraction(MemoryOperation operation, uint16_t address, uint8_t value);

    bool operator==(const MachineCycleInteraction &other) const;
};

std::ostream &operator<<(std::ostream &os, const MachineCycleInteraction &m);

} // namespace GameBoy
