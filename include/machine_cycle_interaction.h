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

struct MachineCycleInteraction {
    MemoryOperation memory_operation;
    uint16_t address_accessed{};
    uint8_t value_written{};

    MachineCycleInteraction(MemoryOperation operation)
        : memory_operation(operation) {
    }

    MachineCycleInteraction(MemoryOperation operation, uint16_t address)
        : memory_operation(operation),
          address_accessed(address) {
    }

    MachineCycleInteraction(MemoryOperation operation, uint16_t address, uint8_t value)
        : memory_operation(operation),
          address_accessed(address),
          value_written(value) {
    }

    bool operator==(const MachineCycleInteraction &other) const {
        if (memory_operation != other.memory_operation) {
            return false;
        }

        switch (memory_operation) {
            case MemoryOperation::None:
                return true;
            case MemoryOperation::Read:
                return address_accessed == other.address_accessed;
            case MemoryOperation::Write:
                return address_accessed == other.address_accessed &&
                       value_written == other.value_written;
        }
    }
};

} // namespace GameBoy
