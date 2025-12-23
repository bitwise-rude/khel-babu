#include "cpu.h"

CPU init_cpu(Memory *p_mem){
    return (CPU) {
        .PC = 0x100,
        .p_memory = p_mem
    };
}

// steps the CPU
void step_cpu(CPU *cpu){
    // fetch from memory
    u8 opcode = memory_read_8(cpu->p_memory, cpu->PC);
    printf("OPCODE FETCHED IS: %x\n", opcode);

    // increment the PC
    cpu->PC += 1;

    // execute the instruction

}