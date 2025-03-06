#include "cpu.h"
#include "memory.h"

int main() {
    GameBoy::Memory memory{};
    GameBoy::CPU cpu{memory};
    cpu.execute_instruction(0x34);
    cpu.print_values();
    cpu.execute_instruction(0x35);
    cpu.print_values();
    return 0;
}
