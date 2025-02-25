#include <iostream>
#include "cpu.h"

namespace GameBoy {
    void CPU::executeInstruction(std::uint8_t opcode) {
        if (opcode != 0xCB) {
            switch (opcode) {
                case 0x00: nop00(); break;
                case 0x01: ld01(); break;
                default:
                    std::cerr << "Unimplemented instruction: 0x" << std::hex << static_cast<int>(opcode) << std::endl;
                    break;
            }
        }
        else {
            std::uint8_t prefixedOpcode = memory[pc + 1];
            switch (prefixedOpcode) {
                default:
                    std::cerr << "Unimplemented instruction: 0xCB" << std::hex << static_cast<int>(prefixedOpcode) << std::endl;
                    break;
            }
        }
    }

    void CPU::nop00() {
        pc += 1;
        cyclesElapsed += 4;
    }

    void CPU::ld01() {
        bc = memory.read16(pc + 1);
        pc += 3;
        cyclesElapsed += 12;
    }
}
