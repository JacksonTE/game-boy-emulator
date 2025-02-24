#include "cpu.h"

int main() {
    GameBoy::CPU cpu;
    cpu.executeInstruction(0x00);
    return 0;
}
