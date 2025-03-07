#include "cpu.h"
#include "memory.h"

int main() {
    GameBoy::Memory memory{};
    GameBoy::CPU cpu{memory};

    for (uint8_t i{0x00}; i <= 0xbf; i++) {
        cpu.execute_instruction(i);
    }
    cpu.print_values();
    return 0;
}
