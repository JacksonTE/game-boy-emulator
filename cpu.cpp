#include <iostream>
#include "cpu.h"

namespace GameBoy {
    const CPU::InstructionFunc CPU::instructionTable[256] = {
        &CPU::nop,
        nullptr,
    };

    void CPU::executeInstruction(std::uint8_t opcode) {
        if (instructionTable[opcode]) {
            instructionTable[opcode](*this);
        }
        else {
            std::cerr << "Unimplemented instruction: 0x" << std::hex << static_cast<int>(opcode) << std::endl;
        }
    }

    void CPU::nop(CPU& cpu) {
        std::cout << "placeholder, nop executing" << std::endl;
    }
}
