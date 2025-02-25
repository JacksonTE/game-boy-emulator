#include "cpu.h"
#include "memory.h"

int main() {
    GameBoy::Memory memory;
    GameBoy::CPU cpu(memory);
    cpu.executeInstruction(0x00);
    cpu.executeInstruction(0x01);
    return 0;
}
