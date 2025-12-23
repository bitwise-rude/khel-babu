#include "cpu.h"


// opcode functions
static inline void nop(struct CPU *cpu){
    // do nothing
}

static Opcode opcodes[256]= {
    {"NOP",0x00,4,0,&nop}
};


// steps the CPU
void step_cpu(CPU *cpu){
    // fetch from memory
    u8 opcode = memory_read_8(cpu->p_memory, cpu->PC);
    printf("OPCODE FETCHED IS: %x\n", opcode);

    // increment the PC
    cpu->PC += 1;

    // execute the instruction
    Opcode to_exec = opcodes[opcode];
    printf("EXECUTING THE INSTRUCTION %s\n\n",to_exec.name);
    to_exec.opcode_method(cpu);


}