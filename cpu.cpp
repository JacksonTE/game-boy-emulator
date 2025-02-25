#include <iomanip>
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

    void CPU::printInternalValues() const {
        std::cout << "================== CPU Registers ==================" << std::endl;
        std::cout << std::hex << std::uppercase;
        std::cout << std::setfill('0');

        std::cout << "AF: 0x" << std::setw(4) << af
                  << "   (A: 0x" << std::setw(2) << static_cast<int>(a)
                  << ", F: 0x" << std::setw(2) << static_cast<int>(f) << ")" << std::endl;

        std::cout << "BC: 0x" << std::setw(4) << bc
                  << "   (B: 0x" << std::setw(2) << static_cast<int>(b)
                  << ", C: 0x" << std::setw(2) << static_cast<int>(c) << ")" << std::endl;

        std::cout << "DE: 0x" << std::setw(4) << de
                  << "   (D: 0x" << std::setw(2) << static_cast<int>(d)
                  << ", E: 0x" << std::setw(2) << static_cast<int>(e) << ")" << std::endl;

        std::cout << "HL: 0x" << std::setw(4) << hl
                  << "   (H: 0x" << std::setw(2) << static_cast<int>(h)
                  << ", L: 0x" << std::setw(2) << static_cast<int>(l) << ")" << std::endl;

        std::cout << "SP: 0x" << std::setw(4) << sp << std::endl;
        std::cout << "PC: 0x" << std::setw(4) << pc << std::endl;

        std::cout << std::dec;
        std::cout << "Cycles Elapsed: " << cyclesElapsed << std::endl;
        std::cout << "===================================================" << std::endl;
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
