#include <format>
#include <ostream>
#include <string>
#include "machine_cycle_interaction.h"

namespace GameBoy {

std::string memory_operation_to_string(MemoryOperation op) {
    switch (op) {
    case MemoryOperation::None:
        return "None";
    case MemoryOperation::Read:
        return "Read";
    case MemoryOperation::Write:
        return "Write";
    default:
        return "Unknown";
    }
}

MachineCycleInteraction::MachineCycleInteraction(MemoryOperation operation)
    : memory_operation(operation) {
}

MachineCycleInteraction::MachineCycleInteraction(MemoryOperation operation, uint16_t address)
    : memory_operation(operation),
      address_accessed(address) {
}

MachineCycleInteraction::MachineCycleInteraction(MemoryOperation operation, uint16_t address, uint8_t value)
    : memory_operation(operation),
      address_accessed(address),
      value_written(value) {
}

bool MachineCycleInteraction::operator==(const MachineCycleInteraction &other) const {
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
        default:
            return false;
    }
}

std::ostream &operator<<(std::ostream &os, const MachineCycleInteraction &m) {
    os << "MachineCycleMemoryInteraction{"
       << "memory_operation: " << memory_operation_to_string(m.memory_operation) << ", "
       << "address_accessed: " << (m.address_accessed ? std::format("{:04x}", m.address_accessed) : "none") << ", "
       << "value_written: " << (m.value_written ? std::format("{:04x}", m.value_written) : "none")
       << "}";
    return os;
}

} // namespace GameBoy
