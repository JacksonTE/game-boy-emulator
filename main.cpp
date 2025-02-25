#include "cpu.h"
#include "memory.h"

int main() {
    GameBoy::Memory memory;
    GameBoy::CPU cpu(memory);
    cpu.printInternalValues();
    cpu.executeInstruction(0x00);
    cpu.printInternalValues();
    cpu.executeInstruction(0x01);
    cpu.printInternalValues();
    return 0;
}
