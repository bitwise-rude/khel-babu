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
     u8 val = memory_read_8(cpu->p_memory,cpu->PC.val);
     cpu->PC.val ++;
     return val;
}

static inline u16 get_next_16(CPU *cpu){
    u8 lo = get_next_8(cpu);
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
    u8 lo = get_next_8(cpu);

    push(cpu, lo);
    push(cpu,hi);

    cpu->PC.val = addr;

}


static inline void ld_r_r_helper(u8 *src, u8 *dst){
    *dst = *src;
}

static inline int flag_z(const CPU *cpu) {
    return (cpu->AF.lo & ZERO) != 0;
}

static inline int flag_n(const CPU *cpu) {
    return (cpu->AF.lo & SUBTRACT) != 0;
}

static inline int flag_h(const CPU *cpu) {
    return (cpu->AF.lo & HALF_CARRY) != 0;
}

static inline int flag_c(const CPU *cpu) {
    return (cpu->AF.lo & CARRY) != 0;
}

/*  Opcode Functions  */
static inline void nop(){
    // do nothing
}

static inline void jp_a16( CPU *cpu){
    // Jump to immediate 16 bit address
   
    cpu -> PC.val =  get_next_16(cpu);
}

static inline void jr_nz(CPU *cpu){
    if(! flag_z(cpu)){
        jp_a16(cpu);
        cpu->cycles += 3;
        return;
    }
    cpu -> cycles += 2;

}

static inline void jr_nc(CPU *cpu){
    if(! flag_c(cpu)){
        jp_a16(cpu);
        cpu->cycles += 3;
        return;
    }
    cpu -> cycles += 2;

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

static inline void ret_nz(CPU *cpu){
    if (! flag_z(cpu)){
        u8 lo = pop(cpu);
        u8 hi = pop(cpu);

        cpu->PC.val = combine_bytes(hi,lo);

        // cycles
        cpu->cycles += 5;
        return;
    }

    cpu->cycles += 2;
}

static inline void ret_nc(CPU *cpu){
    if (! flag_c(cpu)){
        u8 lo = pop(cpu);
        u8 hi = pop(cpu);

        cpu->PC.val = combine_bytes(hi,lo);

        // cycles
        cpu->cycles += 4;
        return ;
        // TODO : maybe a bug in this thing
    }

    cpu->cycles += 3;
}

static inline void ld_b_b(CPU *cpu){
    ld_r_r_helper(&cpu->BC.hi, &cpu->BC.hi);
}

static inline void ld_b_c(CPU *cpu){
    ld_r_r_helper(&cpu->BC.hi, &cpu->BC.lo);
}

static inline void ld_b_d(CPU *cpu){
    ld_r_r_helper(&cpu->BC.hi, &cpu->DE.hi);
}

static inline void ld_b_e(CPU *cpu){
    ld_r_r_helper(&cpu->BC.hi, &cpu->DE.lo);
}

static inline void ld_b_h(CPU *cpu){
    ld_r_r_helper(&cpu->BC.hi, &cpu->HL.hi);
}

static inline void ld_b_l(CPU *cpu){
    ld_r_r_helper(&cpu->BC.hi, &cpu->HL.lo);
}

static inline void ld_b_a(CPU *cpu){
    ld_r_r_helper(&cpu->BC.hi, &cpu->AF.hi);
}

// ld c, reg

static inline void ld_c_b(CPU *cpu){
    ld_r_r_helper(&cpu->BC.lo, &cpu->BC.hi);
}

static inline void ld_c_c(CPU *cpu){
    ld_r_r_helper(&cpu->BC.lo, &cpu->BC.lo);
}

static inline void ld_c_d(CPU *cpu){
    ld_r_r_helper(&cpu->BC.lo, &cpu->DE.hi);
}

static inline void ld_c_e(CPU *cpu){
    ld_r_r_helper(&cpu->BC.lo, &cpu->DE.lo);
}

static inline void ld_c_h(CPU *cpu){
    ld_r_r_helper(&cpu->BC.lo, &cpu->HL.hi);
}

static inline void ld_c_l(CPU *cpu){
    ld_r_r_helper(&cpu->BC.lo, &cpu->HL.lo);
}

static inline void ld_c_a(CPU *cpu){
    ld_r_r_helper(&cpu->BC.lo, &cpu->AF.hi);
}

// ld d, reg
static inline void ld_d_b(CPU *cpu){
    ld_r_r_helper(&cpu->DE.hi, &cpu->BC.hi);
}

static inline void ld_d_c(CPU *cpu){
    ld_r_r_helper(&cpu->DE.hi, &cpu->BC.lo);
}

static inline void ld_d_d(CPU *cpu){
    ld_r_r_helper(&cpu->DE.hi, &cpu->DE.hi);
}

static inline void ld_d_e(CPU *cpu){
    ld_r_r_helper(&cpu->DE.hi, &cpu->DE.lo);
}

static inline void ld_d_h(CPU *cpu){
    ld_r_r_helper(&cpu->DE.hi, &cpu->HL.hi);
}

static inline void ld_d_l(CPU *cpu){
    ld_r_r_helper(&cpu->DE.hi, &cpu->HL.lo);
}

static inline void ld_d_a(CPU *cpu){
    ld_r_r_helper(&cpu->DE.hi, &cpu->AF.hi);

}
// ld e, reg

static inline void ld_e_b(CPU *cpu){
    ld_r_r_helper(&cpu->DE.lo, &cpu->BC.hi);
}

static inline void ld_e_c(CPU *cpu){
    ld_r_r_helper(&cpu->DE.lo, &cpu->BC.lo);
}

static inline void ld_e_d(CPU *cpu){
    ld_r_r_helper(&cpu->DE.lo, &cpu->DE.hi);
}

static inline void ld_e_e(CPU *cpu){
    ld_r_r_helper(&cpu->DE.lo, &cpu->DE.lo);
}

static inline void ld_e_h(CPU *cpu){
    ld_r_r_helper(&cpu->DE.lo, &cpu->HL.hi);
}

static inline void ld_e_l(CPU *cpu){
    ld_r_r_helper(&cpu->DE.lo, &cpu->HL.lo);
}

static inline void ld_e_a(CPU *cpu){
    ld_r_r_helper(&cpu->DE.lo, &cpu->AF.hi);
}

// ld h, reg
static inline void ld_h_b(CPU *cpu){
    ld_r_r_helper(&cpu->HL.hi, &cpu->BC.hi);
}

static inline void ld_h_c(CPU *cpu){
    ld_r_r_helper(&cpu->HL.hi, &cpu->BC.lo);
}

static inline void ld_h_d(CPU *cpu){
    ld_r_r_helper(&cpu->HL.hi, &cpu->DE.hi);
}

static inline void ld_h_e(CPU *cpu){
    ld_r_r_helper(&cpu->HL.hi, &cpu->DE.lo);
}

static inline void ld_h_h(CPU *cpu){
    ld_r_r_helper(&cpu->HL.hi, &cpu->HL.hi);
}

static inline void ld_h_l(CPU *cpu){
    ld_r_r_helper(&cpu->HL.hi, &cpu->HL.lo);
}

static inline void ld_h_a(CPU *cpu){
    ld_r_r_helper( &cpu->HL.hi, &cpu->AF.hi);
}

// ld l, reg

static inline void ld_l_b(CPU *cpu){
    ld_r_r_helper(&cpu->HL.lo, &cpu->BC.hi);
}

static inline void ld_l_c(CPU *cpu){
    ld_r_r_helper(&cpu->HL.lo, &cpu->BC.lo);
}

static inline void ld_l_d(CPU *cpu){
    ld_r_r_helper(&cpu->HL.lo, &cpu->DE.hi);
}

static inline void ld_l_e(CPU *cpu){
    ld_r_r_helper(&cpu->HL.lo, &cpu->DE.lo);
}

static inline void ld_l_h(CPU *cpu){
    ld_r_r_helper(&cpu->HL.lo, &cpu->HL.hi);
}

static inline void ld_l_l(CPU *cpu){
    ld_r_r_helper(&cpu->HL.lo, &cpu->HL.lo);
}

static inline void ld_l_a(CPU *cpu){
    ld_r_r_helper(&cpu->HL.lo, &cpu->AF.hi);
}

// ld a,reg


static inline void ld_a_b(CPU *cpu){
    ld_r_r_helper(&cpu->AF.hi, &cpu->BC.hi);
}

static inline void ld_a_c(CPU *cpu){
    ld_r_r_helper(&cpu->AF.hi, &cpu->BC.lo);
}

static inline void ld_a_d(CPU *cpu){
    ld_r_r_helper( &cpu->AF.hi, &cpu->DE.hi);
}

static inline void ld_a_e(CPU *cpu){
    ld_r_r_helper(&cpu->AF.hi, &cpu->DE.lo);
}

static inline void ld_a_h(CPU *cpu){
    ld_r_r_helper(&cpu->AF.hi, &cpu->HL.hi);
}

static inline void ld_a_l(CPU *cpu){
    ld_r_r_helper( &cpu->AF.hi, &cpu->HL.lo);
}

static inline void ld_a_a(CPU *cpu){
    ld_r_r_helper(&cpu->AF.hi, &cpu->AF.hi);
}

// ld r, M
static inline void ld_b_m(CPU *cpu){
    ld_r_r_helper( &cpu->BC.hi, get_address(cpu->p_memory, cpu->HL.val));
}

static inline void ld_d_m(CPU *cpu){
    ld_r_r_helper(&cpu->DE.hi, get_address(cpu->p_memory, cpu->HL.val));
}

static inline void ld_h_m(CPU *cpu){
    ld_r_r_helper( &cpu->HL.hi, get_address(cpu->p_memory, cpu->HL.val));
}

static inline void ld_c_m(CPU *cpu){
    ld_r_r_helper( &cpu->BC.lo, get_address(cpu->p_memory, cpu->HL.val));
}

static inline void ld_e_m(CPU *cpu){
    ld_r_r_helper(&cpu->DE.lo, get_address(cpu->p_memory, cpu->HL.val));
}

static inline void ld_l_m(CPU *cpu){
    ld_r_r_helper(&cpu->HL.lo, get_address(cpu->p_memory, cpu->HL.val));
}

static inline void ld_a_m(CPU *cpu){
    ld_r_r_helper(&cpu->AF.hi, get_address(cpu->p_memory, cpu->HL.val));
}

// ld M,reg
static inline void ld_m_b(CPU *cpu){
    ld_r_r_helper(get_address(cpu->p_memory, cpu->HL.val), &cpu->BC.hi);
}

static inline void ld_m_a(CPU *cpu){
    ld_r_r_helper(get_address(cpu->p_memory, cpu->HL.val), &cpu->AF.hi);
}

static inline void ld_m_d(CPU *cpu){
    ld_r_r_helper(get_address(cpu->p_memory, cpu->HL.val), &cpu->DE.hi);
}

static inline void ld_m_h(CPU *cpu){
    ld_r_r_helper(get_address(cpu->p_memory, cpu->HL.val), &cpu->HL.hi);
}

static inline void ld_m_c(CPU *cpu){
    ld_r_r_helper(get_address(cpu->p_memory, cpu->HL.val), &cpu->BC.lo);
}

static inline void ld_m_e(CPU *cpu){
    ld_r_r_helper(get_address(cpu->p_memory, cpu->HL.val), &cpu->DE.lo);
}

static inline void ld_m_l(CPU *cpu){
    ld_r_r_helper(get_address(cpu->p_memory, cpu->HL.val), &cpu->HL.lo);
}

static inline void ld_bc_a(CPU *cpu){
    ld_r_r_helper(get_address(cpu->p_memory, cpu->BC.val), &cpu->AF.hi);
}

static inline void ld_de_a(CPU *cpu){
    ld_r_r_helper(get_address(cpu->p_memory, cpu->DE.val), &cpu->AF.hi);
}

static inline void ld_hlp_a(CPU *cpu){
    ld_r_r_helper(get_address(cpu->p_memory, cpu->HL.val), &cpu->AF.hi);
    cpu->HL.val ++;
}

static inline void ld_hlm_a(CPU *cpu){
    ld_r_r_helper(get_address(cpu->p_memory, cpu->HL.val), &cpu->AF.hi);
    cpu->HL.val --;
}

static inline void ld_b_d8(CPU *cpu){
    cpu->BC.hi = get_next_8(cpu);
}

static inline void ld_d_d8(CPU *cpu){
    cpu->DE.hi = get_next_8(cpu);
}

static inline void ld_h_d8(CPU *cpu){
    cpu->HL.hi = get_next_8(cpu);
}

static inline void ld_m_d8(CPU *cpu){
    memory_write(cpu->p_memory, cpu->HL.val, get_next_8(cpu));
}

static inline void ld_a_bc(CPU *cpu){
    cpu->AF.hi = memory_read_8(cpu->p_memory, cpu->BC.val);
}

static inline void ld_a_de(CPU *cpu){
    cpu->AF.hi = memory_read_8(cpu->p_memory, cpu->DE.val);
}

static inline void ld_a_hlp(CPU *cpu){
    cpu->AF.hi = memory_read_8(cpu->p_memory, cpu->HL.val);
    cpu->HL.val ++;
}

static inline void ld_a_hlm(CPU *cpu){
    cpu->AF.hi = memory_read_8(cpu->p_memory, cpu->HL.val);
    cpu->HL.val --;
}
//
static inline void ld_c_d8(CPU *cpu){
    cpu->BC.lo = get_next_8(cpu);
}

static inline void ld_e_d8(CPU *cpu){
    cpu->DE.lo = get_next_8(cpu);
}

static inline void ld_l_d8(CPU *cpu){
    cpu->HL.lo = get_next_8(cpu);
}

static inline void ld_a_d8(CPU *cpu){
    cpu->AF.hi = get_next_8(cpu);
}

static Opcode opcodes[256]= {
    [0] = {"NOP",       4,      &nop},

    [0xc3] = {"JP a16", 4,      &jp_a16},
    [0x20] = {"JR NZ, s8", 0, &jr_nz},
    [0x30] = {"JR NC, s8", 0, &jr_nc},

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
    [0xc0] = {"RET NZ", 0, &ret_nz},
    [0xd0] = {"RET NC", 0, &ret_nc},

    [0x40] = {"LD B, B", 1, &ld_b_b},
    [0x41] = {"LD B, C", 1, &ld_b_c},
    [0x42] = {"LD B, D", 1, &ld_b_d},
    [0x43] = {"LD B, E", 1, &ld_b_e},
    [0x44] = {"LD B, H", 1, &ld_b_h},
    [0x45] = {"LD B, L", 1, &ld_b_l},
    [0x47] = {"LD B, A", 1, &ld_b_a},
    [0x48] = {"LD C, B", 1, &ld_c_b},
    [0x49] = {"LD C, C", 1, &ld_c_c},
    [0x4A] = {"LD C, D", 1, &ld_c_d},
    [0x4B] = {"LD C, E", 1, &ld_c_e},
    [0x4C] = {"LD C, H", 1, &ld_c_h},
    [0x4D] = {"LD C, L", 1, &ld_c_l},
    [0x4F] = {"LD C, A", 1, &ld_c_a},
    [0x46] = {"LD B, (HL)", 2, &ld_b_m},
    [0x4E] = {"LD C, (HL)", 2, &ld_c_m},

    [0x50] = {"LD D, B", 1, &ld_d_b},
    [0x51] = {"LD D, C", 1, &ld_d_c},
    [0x52] = {"LD D, D", 1, &ld_d_d},
    [0x53] = {"LD D, E", 1, &ld_d_e},
    [0x54] = {"LD D, H", 1, &ld_d_h},
    [0x55] = {"LD D, L", 1, &ld_d_l},
    [0x57] = {"LD D, A", 1, &ld_b_a},
    [0x58] = {"LD E, B", 1, &ld_e_b},
    [0x59] = {"LD E, C", 1, &ld_e_c},
    [0x5A] = {"LD E, D", 1, &ld_e_d},
    [0x5B] = {"LD E, E", 1, &ld_e_e},
    [0x5C] = {"LD E, H", 1, &ld_e_h},
    [0x5D] = {"LD E, L", 1, &ld_e_l},
    [0x5F] = {"LD E, A", 1, &ld_e_a},
    [0x56] = {"LD D, (HL)", 2, &ld_d_m},
    [0x5E] = {"LD E, (HL)", 2, &ld_e_m},
    

    [0x61] = {"LD H, C", 1, &ld_h_c},
    [0x60] = {"LD H, B", 1, &ld_h_b},
    [0x62] = {"LD H, D", 1, &ld_h_d},
    [0x63] = {"LD H, E", 1, &ld_h_e},
    [0x64] = {"LD H, H", 1, &ld_h_h},
    [0x65] = {"LD H, L", 1, &ld_h_l},
    [0x67] = {"LD H, A", 1, &ld_h_a},
    [0x68] = {"LD L, B", 1, &ld_l_b},
    [0x69] = {"LD L, C", 1, &ld_l_c},
    [0x6A] = {"LD L, D", 1, &ld_l_d},
    [0x6B] = {"LD L, E", 1, &ld_l_e},
    [0x6C] = {"LD L, H", 1, &ld_l_h},
    [0x6D] = {"LD L, L", 1, &ld_l_l},
    [0x6F] = {"LD L, A", 1, &ld_l_a},
    [0x66] = {"LD H, (HL)", 2, &ld_h_m},
    [0x6E] = {"LD L, (HL)", 2, &ld_l_m},

    [0x70] = {"LD (HL), B", 2, &ld_m_b},
    [0x71] = {"LD (HL), C", 2, &ld_m_c},
    [0x72] = {"LD (HL), D", 2, &ld_m_d},
    [0x73] = {"LD (HL), E", 2, &ld_m_e},
    [0x74] = {"LD (HL), H", 2, &ld_m_h},
    [0x75] = {"LD (HL), L", 2, &ld_m_l},
    [0x77] = {"LD (HL), A", 2, &ld_m_a},
    [0x78] = {"LD A, B", 1, &ld_a_b},
    [0x79] = {"LD A, C", 1, &ld_a_c},
    [0x7A] = {"LD A, D", 1, &ld_a_d},
    [0x7B] = {"LD A, E", 1, &ld_a_e},
    [0x7C] = {"LD A, H", 1, &ld_a_h},
    [0x7D] = {"LD A, L", 1, &ld_a_l},
    [0x7F] = {"LD A, A", 1, &ld_a_a},
    [0x7E] = {"LD A, (HL)", 2, &ld_a_m},

    [0x02] = {"LD (BC), A", 2, &ld_bc_a},
    [0x12] = {"LD (DE), A", 2, &ld_de_a},
    [0x22] = {"LD (HL+), A", 2, &ld_hlp_a},
    [0x32] = {"LD (HL-), A", 2, &ld_hlm_a},


    [0x06] = {"LD B, d8", 2, &ld_b_d8},
    [0x16] = {"LD D, d8", 2, &ld_d_d8},
    [0x26] = {"LD H, d8", 2, &ld_h_d8},
    [0x36] = {"LD (HL), d8", 2, &ld_m_d8},

    [0x0A] = {"LD A, (BC)", 2, &ld_a_bc},
    [0x1A] = {"LD A, (DE)", 2, &ld_a_de},
    [0x2A] = {"LD A, (HL+)", 2, &ld_a_hlp},
    [0x3A] = {"LD A, (HL-)", 2, &ld_a_hlm},

    [0x0E] = {"LD C, d8", 2, &ld_c_d8},
    [0x1E] = {"LD E, d8", 2, &ld_e_d8},
    [0x2E] = {"LD L, d8", 2, &ld_l_d8},
    [0x3E] = {"LD A, d8", 2, &ld_a_d8},
    
};


// steps the CPU
void step_cpu(CPU *cpu){
    // fetch from memory
    int i;
    // scanf("%d",&i);
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