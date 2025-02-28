#include "cpu.h"
#include "memory.h"

int main() {
    GameBoy::Memory memory{};
    GameBoy::CPU cpu{memory};
    cpu.print_internal_values();
    cpu.execute_instruction(0x00);
    cpu.print_internal_values();
    cpu.execute_instruction(0x01);
    cpu.print_internal_values();
    return 0;
}
