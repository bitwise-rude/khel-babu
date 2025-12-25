#include "cpu.h"

/* Helper Macros */
#define low (a) (u8)(a & 0x00FF) 
#define high(a) (u8) ((a & 0xFF00) >> 8)

#define ZERO 0x80
#define SUBTRACT 0x40
#define HALF_CARRY 0x20
#define CARRY 0x10

// combines two bytes into a 16 bit word
static inline u16 combine_bytes(u8 hi, u8 lo)
{
    return ((u16)hi << 8) | lo;
}


CPU init_cpu(Memory *p_mem){
    return (CPU) {
        .PC.val = 0x100,
        .p_memory = p_mem
    };
}

/* Helper functions */
static inline u8 get_next_8(CPU *cpu){
     return memory_read_8(cpu->p_memory,cpu->PC.val);
}

static inline u16 get_next_16(CPU *cpu){
    u8 lo = get_next_8(cpu);
    cpu-> PC.val ++;
    u8 hi = get_next_8(cpu);

    return combine_bytes(hi,lo);
}

static inline void push(CPU *cpu, u8 val){
    cpu-> SP.val --;
    memory_write(cpu->p_memory, cpu->SP.val, val);
}

static inline u8 pop(CPU *cpu){
    u8 val = memory_read_8(cpu->p_memory, cpu->SP.val);
    cpu-> SP.val ++;
    return val;
}

static inline void rst_helper(CPU *cpu, u16 addr){
    u8 hi = get_next_8(cpu);
    cpu->PC.val ++;
    u8 lo = get_next_8(cpu);

    push(cpu, lo);
    push(cpu,hi);

    cpu->PC.val = addr;

}


static inline void ld_r_r_helper(CPU *cpu, u8 *src, u8 *dst){
    *dst = *src;
}


/*  Opcode Functions  */
static inline void nop(){
    // do nothing
}

static inline void jp_a16( CPU *cpu){
    // Jump to immediate 16 bit address
   
    cpu -> PC.val =  get_next_16(cpu);
}

static inline void di( CPU *cpu){
    // disable interrut but resetting the IME flag
    cpu->IME = 0;
}

static inline void ld_sp_16(CPU *cpu){
    // sets LD to the immediate register value
    cpu->SP.val = get_next_16(cpu);
}

static inline void ld_bc_16(CPU *cpu){
    // sets LD to the immediate register value
    cpu->BC.val = get_next_16(cpu);
}

static inline void ld_de_16(CPU *cpu){
    // sets LD to the immediate register value
    cpu->DE.val = get_next_16(cpu);
}

static inline void ld_hl_16(CPU *cpu){
    // sets LD to the immediate register value
    cpu->HL.val = get_next_16(cpu);
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

static inline void inc_a(CPU *cpu)
{
    u8 val = cpu->AF.hi;
    u8 res = val + 1;

    cpu->AF.lo &= ~(ZERO | SUBTRACT | HALF_CARRY);

    // Z flag
    if (res == 0)
        cpu->AF.lo |= ZERO;

    // H flag
    if ((val & 0x0F) == 0x0F)
        cpu->AF.lo |= HALF_CARRY;


    cpu->AF.lo &= 0xF0;

    cpu->AF.hi = res;
}

static inline void inc_c(CPU *cpu)
{
    u8 val = cpu->BC.lo;
    u8 res = val + 1;

    cpu->AF.lo &= ~(ZERO | SUBTRACT | HALF_CARRY);

    // Z flag
    if (res == 0)
        cpu->AF.lo |= ZERO;

    // H flag
    if ((val & 0x0F) == 0x0F)
        cpu->AF.lo |= HALF_CARRY;


    cpu->AF.lo &= 0xF0;

    cpu->BC.lo = res;
}

static inline void inc_e(CPU *cpu)

{
    u8 val = cpu->DE.lo;
    u8 res = val + 1;

    cpu->AF.lo &= ~(ZERO | SUBTRACT | HALF_CARRY);

    // Z flag
    if (res == 0)
        cpu->AF.lo |= ZERO;

    // H flag
    if ((val & 0x0F) == 0x0F)
        cpu->AF.lo |= HALF_CARRY;


    cpu->AF.lo &= 0xF0;

    cpu->DE.lo = res;
}

static inline void inc_l(CPU *cpu)
{
    u8 val = cpu->HL.lo;
    u8 res = val + 1;

    cpu->AF.lo &= ~(ZERO | SUBTRACT | HALF_CARRY);

    // Z flag
    if (res == 0)
        cpu->AF.lo |= ZERO;

    // H flag
    if ((val & 0x0F) == 0x0F)
        cpu->AF.lo |= HALF_CARRY;


    cpu->AF.lo &= 0xF0;

    cpu->HL.lo = res;
}

static inline void ret(CPU *cpu){
    u8 lo = pop(cpu);
    u8 hi = pop(cpu);

    cpu->PC.val = combine_bytes(hi,lo);
}

static inline void ld_b_b(CPU *cpu){
    ld_r_r_helper(cpu, &cpu->BC.hi, &cpu->BC.hi);
}

static inline void ld_b_c(CPU *cpu){
    ld_r_r_helper(cpu, &cpu->BC.hi, &cpu->BC.lo);
}

static inline void ld_b_d(CPU *cpu){
    ld_r_r_helper(cpu, &cpu->BC.hi, &cpu->DE.hi);
}

static inline void ld_b_e(CPU *cpu){
    ld_r_r_helper(cpu, &cpu->BC.hi, &cpu->DE.lo);
}

static inline void ld_b_h(CPU *cpu){
    ld_r_r_helper(cpu, &cpu->BC.hi, &cpu->HL.hi);
}

static inline void ld_b_l(CPU *cpu){
    ld_r_r_helper(cpu, &cpu->BC.hi, &cpu->HL.lo);
}

static inline void ld_b_a(CPU *cpu){
    ld_r_r_helper(cpu, &cpu->BC.hi, &cpu->AF.hi);
}

// ld c, reg

static inline void ld_c_b(CPU *cpu){
    ld_r_r_helper(cpu, &cpu->BC.lo, &cpu->BC.hi);
}

static inline void ld_c_c(CPU *cpu){
    ld_r_r_helper(cpu, &cpu->BC.lo, &cpu->BC.lo);
}

static inline void ld_c_d(CPU *cpu){
    ld_r_r_helper(cpu, &cpu->BC.lo, &cpu->DE.hi);
}

static inline void ld_c_e(CPU *cpu){
    ld_r_r_helper(cpu, &cpu->BC.lo, &cpu->DE.lo);
}

static inline void ld_c_h(CPU *cpu){
    ld_r_r_helper(cpu, &cpu->BC.lo, &cpu->HL.hi);
}

static inline void ld_c_l(CPU *cpu){
    ld_r_r_helper(cpu, &cpu->BC.lo, &cpu->HL.lo);
}

static inline void ld_c_a(CPU *cpu){
    ld_r_r_helper(cpu, &cpu->BC.lo, &cpu->AF.hi);
}

// ld d, reg
static inline void ld_d_b(CPU *cpu){
    ld_r_r_helper(cpu, &cpu->DE.hi, &cpu->BC.hi);
}

static inline void ld_d_c(CPU *cpu){
    ld_r_r_helper(cpu, &cpu->DE.hi, &cpu->BC.lo);
}

static inline void ld_d_d(CPU *cpu){
    ld_r_r_helper(cpu, &cpu->DE.hi, &cpu->DE.hi);
}

static inline void ld_d_e(CPU *cpu){
    ld_r_r_helper(cpu, &cpu->DE.hi, &cpu->DE.lo);
}

static inline void ld_d_h(CPU *cpu){
    ld_r_r_helper(cpu, &cpu->DE.hi, &cpu->HL.hi);
}

static inline void ld_d_l(CPU *cpu){
    ld_r_r_helper(cpu, &cpu->DE.hi, &cpu->HL.lo);
}

static inline void ld_d_a(CPU *cpu){
    ld_r_r_helper(cpu, &cpu->DE.hi, &cpu->AF.hi);
}

// ld e, reg

static inline void ld_e_b(CPU *cpu){
    ld_r_r_helper(cpu, &cpu->DE.lo, &cpu->BC.hi);
}

static inline void ld_e_c(CPU *cpu){
    ld_r_r_helper(cpu, &cpu->DE.lo, &cpu->BC.lo);
}

static inline void ld_e_d(CPU *cpu){
    ld_r_r_helper(cpu, &cpu->DE.lo, &cpu->DE.hi);
}

static inline void ld_e_e(CPU *cpu){
    ld_r_r_helper(cpu, &cpu->DE.lo, &cpu->DE.lo);
}

static inline void ld_e_h(CPU *cpu){
    ld_r_r_helper(cpu, &cpu->DE.lo, &cpu->HL.hi);
}

static inline void ld_e_l(CPU *cpu){
    ld_r_r_helper(cpu, &cpu->DE.lo, &cpu->HL.lo);
}

static inline void ld_e_a(CPU *cpu){
    ld_r_r_helper(cpu, &cpu->DE.lo, &cpu->AF.hi);
}

// ld h, reg
static inline void ld_h_b(CPU *cpu){
    ld_r_r_helper(cpu, &cpu->HL.hi, &cpu->BC.hi);
}

static inline void ld_h_c(CPU *cpu){
    ld_r_r_helper(cpu, &cpu->HL.hi, &cpu->BC.lo);
}

static inline void ld_h_d(CPU *cpu){
    ld_r_r_helper(cpu, &cpu->HL.hi, &cpu->DE.hi);
}

static inline void ld_h_e(CPU *cpu){
    ld_r_r_helper(cpu, &cpu->HL.hi, &cpu->DE.lo);
}

static inline void ld_h_h(CPU *cpu){
    ld_r_r_helper(cpu, &cpu->HL.hi, &cpu->HL.hi);
}

static inline void ld_h_l(CPU *cpu){
    ld_r_r_helper(cpu, &cpu->HL.hi, &cpu->HL.lo);
}

static inline void ld_h_a(CPU *cpu){
    ld_r_r_helper(cpu, &cpu->HL.hi, &cpu->AF.hi);
}

// ld l, reg

static inline void ld_l_b(CPU *cpu){
    ld_r_r_helper(cpu, &cpu->HL.lo, &cpu->BC.hi);
}

static inline void ld_l_c(CPU *cpu){
    ld_r_r_helper(cpu, &cpu->HL.lo, &cpu->BC.lo);
}

static inline void ld_l_d(CPU *cpu){
    ld_r_r_helper(cpu, &cpu->HL.lo, &cpu->DE.hi);
}

static inline void ld_l_e(CPU *cpu){
    ld_r_r_helper(cpu, &cpu->HL.lo, &cpu->DE.lo);
}

static inline void ld_l_h(CPU *cpu){
    ld_r_r_helper(cpu, &cpu->HL.lo, &cpu->HL.hi);
}

static inline void ld_l_l(CPU *cpu){
    ld_r_r_helper(cpu, &cpu->HL.lo, &cpu->HL.lo);
}

static inline void ld_l_a(CPU *cpu){
    ld_r_r_helper(cpu, &cpu->HL.lo, &cpu->AF.hi);
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

    [0x3c] = {"INC A", 1, &inc_a},
    [0x2c] = {"INC L", 1, &inc_l},
    [0x1c] = {"INC E", 1, &inc_e},
    [0x0c] = {"INC C", 1, &inc_c},

    [0xc9] = {"RET", 4, &ret},



};


// steps the CPU
void step_cpu(CPU *cpu){
    // fetch from memory
    u8 opcode = memory_read_8(cpu->p_memory, cpu->PC.val);
    printf("\nOPCODE FETCHED IS:  %.2xH \n",opcode);

    // next instructions
    cpu->PC.val += 1;

    // execute the instruction
    Opcode to_exec = opcodes[opcode];
    if (to_exec.opcode_method != NULL) printf("EXECUTING THE INSTRUCTION %s\n",to_exec.name);
    else {printf("NOT IMPLEMENTED\n"); exit(1);}
    to_exec.opcode_method(cpu);
    cpu->cycles += to_exec.cycles;
    printf("\n");


}