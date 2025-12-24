#include "cpu.h"



static inline u16 combine_bytes(u8 hi, u8 lo)
{
    return ((u16)hi << 8) | lo;
}


CPU init_cpu(Memory *p_mem){
    return (CPU) {
        .PC = 0x100,
        .p_memory = p_mem
    };
}

/* Helper functions */
static inline u8 get_next_8(CPU *cpu){
     return memory_read_8(cpu->p_memory,cpu->PC);
}

static inline u16 get_next_16(CPU *cpu){
    u8 lo = get_next_8(cpu);
    cpu -> PC ++;
    u8 hi = get_next_8(cpu);

    return combine_bytes(hi,lo);
}

static inline void push(CPU *cpu, u8 val){
    cpu-> SP --;
    memory_write(cpu->p_memory, cpu->SP, val);
}

/*  Opcode Functions  */
static inline void nop(){
    // do nothing
}

static inline void jp_a16( CPU *cpu){
    // Jump to immediate 16 bit address
   
    cpu -> PC =  get_next_16(cpu);
}

static inline void di( CPU *cpu){
    // disable interrut but resetting the IME flag
    cpu->IME = 0;
}

static inline void ld_sp_16(CPU *cpu){
    // sets LD to the immediate register value
    cpu->SP = get_next_16(cpu);
}

static inline void ld_bc_16(CPU *cpu){
    // sets LD to the immediate register value
    cpu->BC = get_next_16(cpu);
}

static inline void ld_de_16(CPU *cpu){
    // sets LD to the immediate register value
    cpu->DE = get_next_16(cpu);
}
static inline void ld_hl_16(CPU *cpu){
    // sets LD to the immediate register value
    cpu->HL = get_next_16(cpu);
}

static inline void rst_helper(CPU *cpu, u16 addr){
    u8 hi = get_next_8(cpu);
    cpu->PC ++;
    u8 lo = get_next_8(cpu);

    push(cpu, hi);
    push(cpu,lo);

    cpu->PC = addr;

}

static inline void rst_1(CPU *cpu){
    rst_helper(cpu,0x08);
}

static inline void rst_2(CPU *cpu){
    rst_helper(cpu,0x18);
}

static inline void rst_3(CPU *cpu){
    rst_helper(cpu,0x28);
}

static inline void rst_4(CPU *cpu){
    rst_helper(cpu,0x38);
}


static Opcode opcodes[256]= {
    [0] = {"NOP",       4,      &nop},
    [0xc3] = {"JP a16", 4,      &jp_a16},
    [0xf3] = {"DI",     1,   &di},

    [0x31] = {"LD SP, d16", 3,  &ld_sp_16},
    [0x21] = {"LD HL, d16", 3,  &ld_hl_16},
    [0x11] = {"LD DE, d16", 3,  &ld_de_16},
    [0x01] = {"LD BC, d16", 3,  &ld_bc_16},
    
    [0xcf] = {"RST 1", 4, &rst_1},
    [0xdf] = {"RST 2", 4, &rst_2},
    [0xef] = {"RST 3", 4, &rst_3},
    [0xff] = {"RST 4", 4, &rst_4},


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
    cpu->cycles += to_exec.cycles;
    printf("\n");


}