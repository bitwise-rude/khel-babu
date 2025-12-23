#include "cpu.h"

CPU init_cpu(Memory *p_mem){
    return (CPU) {
        .PC.value = 0x100,
        .p_memory = p_mem
    };
}

// steps the CPU
void step_cpu(CPU *cpu){
    // fetch from memory
    memory_read_8(cpu->p_memory, 0x100);
}