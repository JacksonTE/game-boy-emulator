#include "cpu.h"

int main() {
    GameBoy::CPU cpu;
    cpu.executeInstruction(0x00);
    cpu.executeInstruction(0x01);
    return 0;
}
