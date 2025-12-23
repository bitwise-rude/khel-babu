#include "cpu.h"


// opcode functions
static inline void nop(){
    // do nothing
}

CPU init_cpu(Memory *p_mem){
    return (CPU) {
        .PC = 0x100,
        .p_memory = p_mem
    };
}


static inline void jp_a16( CPU *cpu){
    u8 lo = memory_read_8(cpu->p_memory,cpu->PC);
    cpu -> PC ++;
    u8 hi = memory_read_8(cpu->p_memory,cpu->PC);

    cpu -> PC = (hi << 8) | lo;
}

static inline void di( CPU *cpu){
    cpu->IME = 0;
}


static Opcode opcodes[256]= {
    [0] = {"NOP",4,&nop},
    [0xc3] = {"JP a16",4,&jp_a16},
    [0xf3] = {"DI",1, &di}
};


// steps the CPU
void step_cpu(CPU *cpu){
    // fetch from memory
    u8 opcode = memory_read_8(cpu->p_memory, cpu->PC);
    printf("\nOPCODE FETCHED IS: %o in oct and %x in hex\n", opcode, opcode);

    // next instructions
    cpu->PC += 1;

    // execute the instruction
    Opcode to_exec = opcodes[opcode];
    if (to_exec.opcode_method != NULL) printf("EXECUTING THE INSTRUCTION %s\n\n",to_exec.name);
    else {printf("NOT IMPLEMENTED\n"); exit(1);}
    to_exec.opcode_method(cpu);
    cpu->t_states += to_exec.t_states;
    printf("\n");


}