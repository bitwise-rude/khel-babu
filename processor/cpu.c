#include "cpu.h"
#include "string.h"
#include "../platform/platform.h"
#include "stdlib.h"


/* Helper Macros */
#define low(a) (u8)(a & 0x00FF) 
#define high(a) (u8) ((a & 0xFF00) >> 8)

#define ZERO 0x80
#define SUBTRACT 0x40
#define HALF_CARRY 0x20
#define CARRY 0x10

CPU init_cpu(Memory *p_mem){
    return (CPU) {
        .PC.val = 0x100,
        .p_memory = p_mem,
        .AF.hi = 0x01,
        .AF.lo = 0xB0,
        .BC.hi =	0x00,
        .BC.lo =	0x13,
        .DE.hi = 	0x00,
        .DE.lo =	0xD8,
        .HL.hi = 	0x01,
        .HL.lo = 	0x4D,
        .SP.val = 	0xFFFE,
        .schedule_ei = 0,
        .IME =0,
        .stop_mode = 0,
        #ifdef LOG
        .logs = {0},
        .log_pos = '\0',
        #endif
    };
}

/* Helper functions For Opcodes */

/* Combines two bytes into a 16 bit word */
static inline u16 combine_bytes(u8 hi, u8 lo){ return ((u16)hi << 8) | lo;}

/* Sets Specific Flag */
static inline void set_flag(CPU *cpu, const u8 flag){cpu->AF.lo |= flag;}

/* Resets Specific Flag */
static inline void unset_flag(CPU *cpu, const u8 flag){cpu->AF.lo &= ~(flag);}

/* Next 8 bit value fetched from PC*/
static inline u8 get_next_8(CPU *cpu){
     u8 val = memory_read_8(cpu->p_memory,cpu->PC.val);
     cpu->PC.val ++;
     return val;
}

/* Next 16 bit value fetched from PC + 1 (little endian)*/
static inline u16 get_next_16(CPU *cpu){
    u8 lo = get_next_8(cpu);
    u8 hi = get_next_8(cpu);

    return combine_bytes(hi,lo);
}

static inline void add_hl_helper(CPU *cpu, u16 value){
    u32 hl = cpu->HL.val;
    u32 res = hl + value;

    if ( ((hl & 0x0FFF) + (value & 0x0FFF)) > 0x0FFF )
        set_flag(cpu, HALF_CARRY);
    else
        unset_flag(cpu, HALF_CARRY);

    if (res & 0x10000)
        set_flag(cpu, CARRY);
    else
        unset_flag(cpu, CARRY);

    cpu->HL.val = (u16)res;

    unset_flag(cpu, SUBTRACT);
}


static inline void dec_helper(CPU *cpu, u8 *reg){
    u8 prev = *reg;
    u8 result = prev - 1;

    *reg = result;

    if (result == 0)
        set_flag(cpu, ZERO);
    else
        unset_flag(cpu, ZERO);

    set_flag(cpu, SUBTRACT);

    if ((prev & 0x0F) == 0x00)
        set_flag(cpu, HALF_CARRY);
    else
        unset_flag(cpu, HALF_CARRY);
}

/* Push in the stack */
void push(CPU *cpu, u8 val){
    cpu-> SP.val --;
    memory_write(cpu->p_memory, cpu->SP.val, val);
}

/* POP From the stack */
static inline u8 pop(CPU *cpu){
    u8 val = memory_read_8(cpu->p_memory, cpu->SP.val);
    cpu-> SP.val ++;
    return val;
}

static inline void call_helper(CPU *cpu){
    u16 addr = get_next_16(cpu);   

    push(cpu, cpu->PC.hi);
    push(cpu, cpu->PC.lo);

    cpu->PC.val = addr;
    cpu->cycles += 6;
}

void rst_helper(CPU *cpu, u16 addr){
    u16 pc = cpu->PC.val;      

    push(cpu, pc >> 8);       
    push(cpu, pc & 0xFF);     

    cpu->PC.val = addr;       
}

/* This  can be made better */
static inline void ld_r_r_helper(u8 *dst, u8 *src){ *dst = *src; }

/* Following flag return status of  corresponding Flag */
static inline int flag_z(const CPU *cpu) {return (cpu->AF.lo & ZERO) != 0;}
static inline int flag_n(const CPU *cpu) {return (cpu->AF.lo & SUBTRACT) != 0;}
static inline int flag_h(const CPU *cpu) {return (cpu->AF.lo & HALF_CARRY) != 0;}
static inline int flag_c(const CPU *cpu) {return (cpu->AF.lo & CARRY) != 0;}

static inline void sub_helper(CPU *cpu, const u8 operand){
    u8 prev = cpu->AF.hi;
    u8 new_value = prev - operand;

    cpu->AF.hi = new_value;

    (new_value == 0) ? set_flag(cpu, ZERO): unset_flag(cpu, ZERO);
    set_flag(cpu,SUBTRACT);
    if ((prev & 0x0F) < (operand & 0x0F))
        set_flag(cpu, HALF_CARRY);
    else
        unset_flag(cpu, HALF_CARRY);

    if (prev < operand)
        set_flag(cpu, CARRY);
    else
        unset_flag(cpu, CARRY);
}

static inline void add_a_helper(CPU *cpu,  const u8 operand){
    u8 prev = cpu->AF.hi;
    u8 new_value = prev + operand;

    cpu->AF.hi = new_value;

    (new_value == 0) ? set_flag(cpu, ZERO): unset_flag(cpu, ZERO);

    unset_flag(cpu,SUBTRACT);

    if (((prev & 0x0F) + (operand & 0x0F)) > 0x0F)
        set_flag(cpu, HALF_CARRY);
    else
        unset_flag(cpu, HALF_CARRY);

    if ((u16)prev + (u16)operand > 0xFF)
        set_flag(cpu, CARRY);
    else
        unset_flag(cpu, CARRY);
}

static inline void and_helper(CPU *cpu , const u8 operand){
    u8 result = cpu->AF.hi & operand;
    (result == 0) ? set_flag(cpu, ZERO): unset_flag(cpu, ZERO);
    unset_flag(cpu,SUBTRACT);
    set_flag (cpu, HALF_CARRY); // documented behaviour but not sure TODO
    unset_flag(cpu, CARRY);
    cpu->AF.hi = result;
}

static inline void or_helper(CPU *cpu, const u8 operand){
    u8 result = cpu->AF.hi | operand;

    (result == 0) ? set_flag(cpu, ZERO): unset_flag(cpu, ZERO);
    unset_flag(cpu, SUBTRACT);
    unset_flag(cpu, HALF_CARRY);
    unset_flag(cpu, CARRY);

    cpu->AF.hi = result;
}

static inline void adc_helper(CPU *cpu, const u8 operand){
    u8 prev = cpu->AF.hi;
    u8 carry_in = (cpu->AF.lo & CARRY) ? 1 : 0;

    u16 sum = (u16)prev + (u16)operand + carry_in;
    u8 new_value = (u8)sum;

    cpu->AF.hi = new_value;

    (new_value == 0) ? set_flag(cpu, ZERO) : unset_flag(cpu, ZERO);

    unset_flag(cpu, SUBTRACT);

    if (((prev & 0x0F) + (operand & 0x0F) + carry_in) > 0x0F)
        set_flag(cpu, HALF_CARRY);
    else
        unset_flag(cpu, HALF_CARRY);

    if (sum > 0xFF)
        set_flag(cpu, CARRY);
    else
        unset_flag(cpu, CARRY);
}

static inline void sbc_helper(CPU *cpu, const u8 operand){
    u8 prev = cpu->AF.hi;
    u8 carry_in = (cpu->AF.lo & CARRY) ? 1 : 0;

    u16 diff = (u16)prev - (u16)operand - carry_in;
    u8 new_value = (u8)diff;

    cpu->AF.hi = new_value;
    (new_value == 0) ? set_flag(cpu, ZERO) : unset_flag(cpu, ZERO);

    set_flag(cpu, SUBTRACT);
    if ((prev & 0x0F) < ((operand & 0x0F) + carry_in))
        set_flag(cpu, HALF_CARRY);
    else
        unset_flag(cpu, HALF_CARRY);
    if (prev < (operand + carry_in))
        set_flag(cpu, CARRY);
    else
        unset_flag(cpu, CARRY);
}

static inline void xor_helper(CPU *cpu , const u8 operand){
    u8 result = cpu->AF.hi ^ operand;

    if (result == 0)
        set_flag(cpu,ZERO);
    else
        unset_flag(cpu,ZERO);
    unset_flag(cpu,SUBTRACT);
    unset_flag (cpu, HALF_CARRY); 
    unset_flag(cpu, CARRY);

    cpu->AF.hi = result;
}

static inline void cp_helper(CPU *cpu, const u8 operand){
    u8 prev = cpu->AF.hi;
    u8 result = prev - operand;

    (result == 0) ? set_flag(cpu, ZERO) : unset_flag(cpu, ZERO);

    set_flag(cpu, SUBTRACT);

    if ((prev & 0x0F) < (operand & 0x0F))
        set_flag(cpu, HALF_CARRY);
    else
        unset_flag(cpu, HALF_CARRY);

    if (prev < operand)
        set_flag(cpu, CARRY);
    else
        unset_flag(cpu, CARRY);
}

static inline void jr_helper(CPU *cpu){
    s8 offset = (s8)get_next_8(cpu);   
    cpu->PC.val += offset;
}


/*  
         Opcode Functions  
         Each Opcode has its own associated Function (inline)
*/

static inline void nop(){/* do nothing*/}

// jp ins
static inline void jp_a16( CPU *cpu){   cpu -> PC.val =  get_next_16(cpu);}
static inline void jp_hl(CPU *cpu){     cpu-> PC.val = cpu->HL.val; }

static inline void jp_nz_a16(CPU *cpu){
    u16 addr = get_next_16(cpu);

    if (!flag_z(cpu)){
        cpu->PC.val = addr;
        cpu->cycles += 4;
        return;
    }
    cpu->cycles += 3;
}

static inline void jp_z_a16(CPU *cpu){
    u16 addr = get_next_16(cpu);

    if (flag_z(cpu)){
        cpu->PC.val = addr;
        cpu->cycles += 4;
        return;
    }
    cpu->cycles += 3;
}

static inline void jp_nc_a16(CPU *cpu){
    u16 addr = get_next_16(cpu);

    if (!flag_c(cpu)){
        cpu->PC.val = addr;
        cpu->cycles += 4;
        return;
    }
    cpu->cycles += 3;
}

static inline void jp_c_a16(CPU *cpu){
    u16 addr = get_next_16(cpu);

    if (flag_c(cpu)){
        cpu->PC.val = addr;
        cpu->cycles += 4;
        return;
    }
    cpu->cycles += 3;
}

static inline void jr_s8(CPU *cpu){
    jr_helper(cpu);
    cpu->cycles += 3;
}

static inline void jr_z(CPU *cpu){
    if(flag_z(cpu)){
        jr_helper(cpu);
        cpu->cycles += 3;
        return;
    }
    cpu->PC.val ++;
    cpu -> cycles += 2;
}

static inline void jr_nz(CPU *cpu){
    if(!flag_z(cpu)){
        jr_helper(cpu);
        cpu->cycles += 3;
        return;
    }
    cpu->PC.val ++;
    cpu -> cycles += 2;
}

// call ins
static inline void call_a16(CPU *cpu){  call_helper(cpu);   }

static inline void call_nz_a16(CPU *cpu){
    if(!flag_z(cpu)){
        call_helper(cpu);
        cpu->cycles += 6;
        return;
    }
    cpu->PC.val +=2; 
    cpu -> cycles += 3;
}

static inline void call_nc_a16(CPU *cpu){
    if(!flag_c(cpu)){
        call_helper(cpu);
        cpu->cycles += 6;
        return;
    }
    cpu->PC.val +=2; 
    cpu -> cycles += 3;
}

static inline void call_z_a16(CPU *cpu){
    if(flag_z(cpu)){
        call_helper(cpu);
        cpu->cycles += 6;
        return;
    }
    cpu->PC.val +=2; 
    cpu -> cycles += 3;
}

static inline void call_c_a16(CPU *cpu){
    if(flag_c(cpu)){
        call_helper(cpu);
        cpu->cycles += 6;
        return;
    }
    cpu->PC.val +=2; 
    cpu -> cycles += 3;
}

static inline void jr_nc(CPU *cpu){
    if(! flag_c(cpu)){
        jr_helper(cpu);
        cpu->cycles += 3;
        return;
    }
    cpu->PC.val ++;
    cpu -> cycles += 2;
}

static inline void jr_c(CPU *cpu){
    if(flag_c(cpu)){
        jr_helper(cpu);
        cpu->cycles += 3;
        return;
    }
    cpu->PC.val ++;
    cpu -> cycles += 2;
}

// ADD instructions with mix of others similar kind

static inline void add_hl_bc(CPU *cpu){ add_hl_helper(cpu, cpu->BC.val);   }
static inline void add_hl_de(CPU *cpu){ add_hl_helper(cpu, cpu->DE.val);    }
static inline void add_hl_hl(CPU *cpu){ add_hl_helper(cpu, cpu->HL.val);    }
static inline void add_hl_sp(CPU *cpu){ add_hl_helper(cpu, cpu->SP.val);    }

static inline void daa(CPU *cpu){
    /* This one is a weird and confusing one*/
    u8 a = cpu->AF.hi;
    u8 adjust = 0;
    u8 carry = 0;

    if (!flag_n(cpu)) {
        if (flag_h(cpu) || (a & 0x0F) > 9)
            adjust |= 0x06;

        if (flag_c(cpu) || a > 0x99) {
            adjust |= 0x60;
            carry = 1;
        }

        a += adjust;
    } else {
        if (flag_h(cpu))
            adjust |= 0x06;

        if (flag_c(cpu))
            adjust |= 0x60;

        a -= adjust;
    }

    cpu->AF.hi = a;

    if (a == 0)
        set_flag(cpu, ZERO);
    else
        unset_flag(cpu, ZERO);

    unset_flag(cpu, HALF_CARRY);

    if (!flag_n(cpu)) {
        if (carry)
            set_flag(cpu, CARRY);
        else
            unset_flag(cpu, CARRY);
    }
}

static inline void scf(CPU *cpu){
    set_flag(cpu, CARRY);
    unset_flag(cpu, SUBTRACT);
    unset_flag(cpu, HALF_CARRY);
}

static inline void di( CPU *cpu){
    /* Contrary to EI, this happens during its own execution */
    cpu->IME = 0;
}

// RST instructions
static inline void rst_1(CPU *cpu){ rst_helper(cpu,0x08);}
static inline void rst_2(CPU *cpu){ rst_helper(cpu,0x18);}
static inline void rst_3(CPU *cpu){ rst_helper(cpu,0x28);}
static inline void rst_4(CPU *cpu){ rst_helper(cpu,0x38);}

// INC instructions
static inline void inc_a(CPU *cpu)
{
    u8 val = cpu->AF.hi;
    u8 res = val + 1;

    cpu->AF.lo &= ~(ZERO | SUBTRACT | HALF_CARRY);

    if (res == 0)
        cpu->AF.lo |= ZERO;

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

    if (res == 0)
        cpu->AF.lo |= ZERO;

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

    if (res == 0)
        cpu->AF.lo |= ZERO;

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

    if (res == 0)
        cpu->AF.lo |= ZERO;

    if ((val & 0x0F) == 0x0F)
        cpu->AF.lo |= HALF_CARRY;
    cpu->AF.lo &= 0xF0;
    cpu->HL.lo = res;
}

static inline void inc_b(CPU *cpu)
{
    u8 val = cpu->BC.hi;
    u8 res = val + 1;

    cpu->AF.lo &= ~(ZERO | SUBTRACT | HALF_CARRY);

    if (res == 0)
        cpu->AF.lo |= ZERO;

    if ((val & 0x0F) == 0x0F)
        cpu->AF.lo |= HALF_CARRY;
    cpu->AF.lo &= 0xF0;
    cpu->BC.hi = res;
}

static inline void inc_d(CPU *cpu)
{
    u8 val = cpu->DE.hi;
    u8 res = val + 1;

    cpu->AF.lo &= ~(ZERO | SUBTRACT | HALF_CARRY);

    if (res == 0)
        cpu->AF.lo |= ZERO;

    if ((val & 0x0F) == 0x0F)
        cpu->AF.lo |= HALF_CARRY;

    cpu->AF.lo &= 0xF0;
    cpu->DE.hi = res;
}

static inline void inc_h(CPU *cpu)
{
    u8 val = cpu->HL.hi;
    u8 res = val + 1;

    cpu->AF.lo &= ~(ZERO | SUBTRACT | HALF_CARRY);

    if (res == 0)
        cpu->AF.lo |= ZERO;
    if ((val & 0x0F) == 0x0F)
        cpu->AF.lo |= HALF_CARRY;
    cpu->AF.lo &= 0xF0;
    cpu->HL.hi = res;
}

static inline void inc_m(CPU *cpu)
{
    u8 val = memory_read_8(cpu->p_memory, cpu->HL.val);
    u8 res = val + 1;

    cpu->AF.lo &= ~(ZERO | SUBTRACT | HALF_CARRY);
    if (res == 0)
        cpu->AF.lo |= ZERO;
    if ((val & 0x0F) == 0x0F)
        cpu->AF.lo |= HALF_CARRY;

    cpu->AF.lo &= 0xF0;
    memory_write(cpu->p_memory, cpu->HL.val, res);
}

// TODO: make a helper function for inc and dec 

// RET instructions
static inline void ret(CPU *cpu){
    u8 lo = pop(cpu);
    u8 hi = pop(cpu);
    cpu->PC.val = combine_bytes(hi,lo);
}

static inline void reti(CPU *cpu){
    u8 lo = pop(cpu);
    u8 hi = pop(cpu);
    cpu->PC.val = combine_bytes(hi,lo);

    cpu->IME = 1;
}



static inline void ret_nz(CPU *cpu){
    if (! flag_z(cpu)){
        u8 lo = pop(cpu);
        u8 hi = pop(cpu);
        cpu->PC.val = combine_bytes(hi,lo);
        cpu->cycles += 5;
        return;
    }
    cpu->cycles += 2;
}
static inline void ret_z(CPU *cpu){
    if ( flag_z(cpu)){
        u8 lo = pop(cpu);
        u8 hi = pop(cpu);

        cpu->PC.val = combine_bytes(hi,lo);
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
        cpu->cycles += 4;
        return ;
    }
    cpu->cycles += 3;
}

static inline void ret_c(CPU *cpu){
    if (flag_c(cpu)){
        u8 lo = pop(cpu);
        u8 hi = pop(cpu);

        cpu->PC.val = combine_bytes(hi,lo);
        cpu->cycles += 4;
        return ;
    }
    cpu->cycles += 3;
}

// LD insturcitons
static inline void ld_sp_16(CPU *cpu){ cpu->SP.val = get_next_16(cpu); }
static inline void ld_bc_16(CPU *cpu){ cpu->BC.val = get_next_16(cpu); }
static inline void ld_de_16(CPU *cpu){ cpu->DE.val = get_next_16(cpu); }
static inline void ld_hl_16(CPU *cpu){ cpu->HL.val = get_next_16(cpu); }

static inline void ld_b_b(CPU *cpu){ ld_r_r_helper(&cpu->BC.hi, &cpu->BC.hi);}
static inline void ld_b_c(CPU *cpu){ ld_r_r_helper(&cpu->BC.hi, &cpu->BC.lo);}
static inline void ld_b_d(CPU *cpu){ ld_r_r_helper(&cpu->BC.hi, &cpu->DE.hi);}
static inline void ld_b_e(CPU *cpu){ ld_r_r_helper(&cpu->BC.hi, &cpu->DE.lo);}
static inline void ld_b_h(CPU *cpu){ ld_r_r_helper(&cpu->BC.hi, &cpu->HL.hi);}
static inline void ld_b_l(CPU *cpu){ ld_r_r_helper(&cpu->BC.hi, &cpu->HL.lo);}
static inline void ld_b_a(CPU *cpu){ ld_r_r_helper(&cpu->BC.hi, &cpu->AF.hi);}

static inline void ld_c_b(CPU *cpu){ ld_r_r_helper(&cpu->BC.lo, &cpu->BC.hi);}
static inline void ld_c_c(CPU *cpu){ ld_r_r_helper(&cpu->BC.lo, &cpu->BC.lo);}
static inline void ld_c_d(CPU *cpu){ ld_r_r_helper(&cpu->BC.lo, &cpu->DE.hi);}
static inline void ld_c_e(CPU *cpu){ ld_r_r_helper(&cpu->BC.lo, &cpu->DE.lo);}
static inline void ld_c_h(CPU *cpu){ ld_r_r_helper(&cpu->BC.lo, &cpu->HL.hi);}
static inline void ld_c_l(CPU *cpu){ ld_r_r_helper(&cpu->BC.lo, &cpu->HL.lo);}
static inline void ld_c_a(CPU *cpu){ ld_r_r_helper(&cpu->BC.lo, &cpu->AF.hi);}

static inline void ld_d_b(CPU *cpu){ ld_r_r_helper(&cpu->DE.hi, &cpu->BC.hi);}
static inline void ld_d_c(CPU *cpu){ ld_r_r_helper(&cpu->DE.hi, &cpu->BC.lo);}
static inline void ld_d_d(CPU *cpu){ ld_r_r_helper(&cpu->DE.hi, &cpu->DE.hi);}
static inline void ld_d_e(CPU *cpu){ ld_r_r_helper(&cpu->DE.hi, &cpu->DE.lo);}
static inline void ld_d_h(CPU *cpu){ ld_r_r_helper(&cpu->DE.hi, &cpu->HL.hi);}
static inline void ld_d_l(CPU *cpu){ ld_r_r_helper(&cpu->DE.hi, &cpu->HL.lo);}
static inline void ld_d_a(CPU *cpu){ ld_r_r_helper(&cpu->DE.hi, &cpu->AF.hi);}

static inline void ld_e_b(CPU *cpu){ ld_r_r_helper(&cpu->DE.lo, &cpu->BC.hi);}
static inline void ld_e_c(CPU *cpu){ ld_r_r_helper(&cpu->DE.lo, &cpu->BC.lo);}
static inline void ld_e_d(CPU *cpu){ ld_r_r_helper(&cpu->DE.lo, &cpu->DE.hi);}
static inline void ld_e_e(CPU *cpu){ ld_r_r_helper(&cpu->DE.lo, &cpu->DE.lo);}
static inline void ld_e_h(CPU *cpu){ ld_r_r_helper(&cpu->DE.lo, &cpu->HL.hi);}
static inline void ld_e_l(CPU *cpu){ ld_r_r_helper(&cpu->DE.lo, &cpu->HL.lo);}
static inline void ld_e_a(CPU *cpu){ ld_r_r_helper(&cpu->DE.lo, &cpu->AF.hi);}

static inline void ld_h_b(CPU *cpu){ ld_r_r_helper(&cpu->HL.hi, &cpu->BC.hi);}
static inline void ld_h_c(CPU *cpu){ ld_r_r_helper(&cpu->HL.hi, &cpu->BC.lo);}
static inline void ld_h_d(CPU *cpu){ ld_r_r_helper(&cpu->HL.hi, &cpu->DE.hi);}
static inline void ld_h_e(CPU *cpu){ ld_r_r_helper(&cpu->HL.hi, &cpu->DE.lo);}
static inline void ld_h_h(CPU *cpu){ ld_r_r_helper(&cpu->HL.hi, &cpu->HL.hi);}
static inline void ld_h_l(CPU *cpu){ ld_r_r_helper(&cpu->HL.hi, &cpu->HL.lo);}
static inline void ld_h_a(CPU *cpu){ ld_r_r_helper( &cpu->HL.hi, &cpu->AF.hi);}

static inline void ld_l_b(CPU *cpu){   ld_r_r_helper(&cpu->HL.lo, &cpu->BC.hi);}
static inline void ld_l_c(CPU *cpu){   ld_r_r_helper(&cpu->HL.lo, &cpu->BC.lo);}
static inline void ld_l_d(CPU *cpu){   ld_r_r_helper(&cpu->HL.lo, &cpu->DE.hi);}
static inline void ld_l_e(CPU *cpu){   ld_r_r_helper(&cpu->HL.lo, &cpu->DE.lo);}
static inline void ld_l_h(CPU *cpu){   ld_r_r_helper(&cpu->HL.lo, &cpu->HL.hi);}
static inline void ld_l_l(CPU *cpu){   ld_r_r_helper(&cpu->HL.lo, &cpu->HL.lo);}
static inline void ld_l_a(CPU *cpu){   ld_r_r_helper(&cpu->HL.lo, &cpu->AF.hi);}

static inline void ld_a_b(CPU *cpu){   ld_r_r_helper(&cpu->AF.hi, &cpu->BC.hi);}
static inline void ld_a_c(CPU *cpu){   ld_r_r_helper(&cpu->AF.hi, &cpu->BC.lo);}
static inline void ld_a_d(CPU *cpu){   ld_r_r_helper( &cpu->AF.hi, &cpu->DE.hi);}
static inline void ld_a_e(CPU *cpu){   ld_r_r_helper(&cpu->AF.hi, &cpu->DE.lo);}
static inline void ld_a_h(CPU *cpu){   ld_r_r_helper(&cpu->AF.hi, &cpu->HL.hi);}
static inline void ld_a_l(CPU *cpu){   ld_r_r_helper( &cpu->AF.hi, &cpu->HL.lo);}
static inline void ld_a_a(CPU *cpu){   ld_r_r_helper(&cpu->AF.hi, &cpu->AF.hi);}

static inline void ld_b_m(CPU *cpu){   ld_r_r_helper( &cpu->BC.hi, get_address(cpu->p_memory, cpu->HL.val));}
static inline void ld_d_m(CPU *cpu){   ld_r_r_helper(&cpu->DE.hi, get_address(cpu->p_memory, cpu->HL.val));}
static inline void ld_h_m(CPU *cpu){   ld_r_r_helper( &cpu->HL.hi, get_address(cpu->p_memory, cpu->HL.val));}
static inline void ld_c_m(CPU *cpu){   ld_r_r_helper( &cpu->BC.lo, get_address(cpu->p_memory, cpu->HL.val));}
static inline void ld_e_m(CPU *cpu){   ld_r_r_helper(&cpu->DE.lo, get_address(cpu->p_memory, cpu->HL.val));}
static inline void ld_l_m(CPU *cpu){   ld_r_r_helper(&cpu->HL.lo, get_address(cpu->p_memory, cpu->HL.val));}
static inline void ld_a_m(CPU *cpu){   ld_r_r_helper(&cpu->AF.hi, get_address(cpu->p_memory, cpu->HL.val));}

static inline void ld_m_b(CPU *cpu){   ld_r_r_helper(get_address(cpu->p_memory, cpu->HL.val), &cpu->BC.hi);}
static inline void ld_m_a(CPU *cpu){   ld_r_r_helper(get_address(cpu->p_memory, cpu->HL.val), &cpu->AF.hi);}
static inline void ld_m_d(CPU *cpu){   ld_r_r_helper(get_address(cpu->p_memory, cpu->HL.val), &cpu->DE.hi);}
static inline void ld_m_h(CPU *cpu){   ld_r_r_helper(get_address(cpu->p_memory, cpu->HL.val), &cpu->HL.hi);}
static inline void ld_m_c(CPU *cpu){   ld_r_r_helper(get_address(cpu->p_memory, cpu->HL.val), &cpu->BC.lo);}
static inline void ld_m_e(CPU *cpu){   ld_r_r_helper(get_address(cpu->p_memory, cpu->HL.val), &cpu->DE.lo);}

static inline void ld_sp_hl(CPU *cpu){ cpu->SP.val = cpu->HL.val;}

static inline void ld_hl_sp_s8(CPU *cpu){
    s8 si8 = (s8) get_next_8(cpu);
    u16 sp = cpu->SP.val;
    u16 res = sp + si8;

    if ( ((sp & 0x0F) + (si8 & 0x0F)) > 0x0F )
        set_flag(cpu, HALF_CARRY);
    else
        unset_flag(cpu, HALF_CARRY);

    if ( ((sp & 0xFF) + (si8 & 0xFF)) > 0xFF )
        set_flag(cpu, CARRY);
    else
        unset_flag(cpu, CARRY);

    cpu->HL.val = res;

    unset_flag(cpu, ZERO);
    unset_flag(cpu, SUBTRACT);
}

static inline void ld_a16_sp(CPU *cpu){
    u16 operand = get_next_16(cpu);
    memory_write(cpu->p_memory, operand,cpu->SP.lo);
    memory_write(cpu->p_memory, operand+1, cpu->SP.hi);
}

static inline void ld_a8_a(CPU *cpu){
    u8 operand = get_next_8(cpu);
    u16 actual_adress = 0xFF00 | operand;
    memory_write(cpu->p_memory, actual_adress, cpu->AF.hi);
    // int i;
    // printf("WRITING %x %x", actual_adress, operand);
    // scanf("%d",&i);

}

static inline void ld_mc_a(CPU *cpu){
    u8 operand = cpu->BC.lo;
    u16 actual_adress = 0xFF00 | operand;
    memory_write(cpu->p_memory, actual_adress, cpu->AF.hi);
}

static inline void ld_a_mc(CPU *cpu){
    u8 operand = cpu->BC.lo;
    u16 actual_adress = 0xFF00 | operand;
    cpu->AF.hi = memory_read_8(cpu->p_memory, actual_adress);
}

static inline void ld_a_a8(CPU *cpu){
    u8 operand = get_next_8(cpu);
    u16 actual_adress = 0xFF00 | operand;
    cpu->AF.hi = memory_read_8(cpu->p_memory, actual_adress);
}

static inline void ld_a16_a(CPU *cpu){
    u16 adderess = get_next_16(cpu);
    memory_write(cpu->p_memory, adderess, cpu->AF.hi);
}

static inline void ld_a_a16(CPU *cpu){
    u16 adderess = get_next_16(cpu);
    cpu->AF.hi = memory_read_8(cpu->p_memory, adderess);
}

static inline void ld_m_l(CPU *cpu){ ld_r_r_helper(get_address(cpu->p_memory, cpu->HL.val), &cpu->HL.lo);}
static inline void ld_bc_a(CPU *cpu){ ld_r_r_helper(get_address(cpu->p_memory, cpu->BC.val), &cpu->AF.hi);}
static inline void ld_de_a(CPU *cpu){ ld_r_r_helper(get_address(cpu->p_memory, cpu->DE.val), &cpu->AF.hi);}

static inline void ld_hlp_a(CPU *cpu){
    ld_r_r_helper(get_address(cpu->p_memory, cpu->HL.val), &cpu->AF.hi);
    cpu->HL.val ++;
}

static inline void ld_hlm_a(CPU *cpu){
    ld_r_r_helper(get_address(cpu->p_memory, cpu->HL.val), &cpu->AF.hi);
    cpu->HL.val --;
}

static inline void ld_b_d8(CPU *cpu){ cpu->BC.hi = get_next_8(cpu); }
static inline void ld_d_d8(CPU *cpu){  cpu->DE.hi = get_next_8(cpu);}
static inline void ld_h_d8(CPU *cpu){  cpu->HL.hi = get_next_8(cpu);}
static inline void ld_m_d8(CPU *cpu){  memory_write(cpu->p_memory, cpu->HL.val, get_next_8(cpu));}
static inline void ld_a_bc(CPU *cpu){  cpu->AF.hi = memory_read_8(cpu->p_memory, cpu->BC.val);}
static inline void ld_a_de(CPU *cpu){  cpu->AF.hi = memory_read_8(cpu->p_memory, cpu->DE.val);}

static inline void ld_a_hlp(CPU *cpu){
    cpu->AF.hi = memory_read_8(cpu->p_memory, cpu->HL.val);
    cpu->HL.val ++;
}

static inline void ld_a_hlm(CPU *cpu){
    cpu->AF.hi = memory_read_8(cpu->p_memory, cpu->HL.val);
    cpu->HL.val --;
}

static inline void ld_c_d8(CPU *cpu){ cpu->BC.lo = get_next_8(cpu);}
static inline void ld_e_d8(CPU *cpu){ cpu->DE.lo = get_next_8(cpu);}
static inline void ld_l_d8(CPU *cpu){ cpu->HL.lo = get_next_8(cpu);}
static inline void ld_a_d8(CPU *cpu){ cpu->AF.hi = get_next_8(cpu);}

// sub operation 
static inline void sub_b(CPU *cpu){  sub_helper(cpu, cpu->BC.hi);}
static inline void sub_c(CPU *cpu){  sub_helper(cpu, cpu->BC.lo);}
static inline void sub_d(CPU *cpu){  sub_helper(cpu, cpu->DE.hi);}
static inline void sub_e(CPU *cpu){  sub_helper(cpu, cpu->DE.lo);}
static inline void sub_h(CPU *cpu){  sub_helper(cpu, cpu->HL.hi);}
static inline void sub_l(CPU *cpu){  sub_helper(cpu, cpu->HL.lo);}
static inline void sub_a(CPU *cpu){  sub_helper(cpu, cpu->AF.hi);}
static inline void sub_m(CPU *cpu){  sub_helper(cpu, memory_read_8(cpu->p_memory,cpu->HL.val));}

// add operation
static inline void add_a_b(CPU *cpu){  add_a_helper(cpu, cpu->BC.hi);}
static inline void add_a_c(CPU *cpu){  add_a_helper(cpu, cpu->BC.lo);}
static inline void add_a_d(CPU *cpu){  add_a_helper(cpu, cpu->DE.hi);}
static inline void add_a_e(CPU *cpu){  add_a_helper(cpu, cpu->DE.lo);}
static inline void add_a_h(CPU *cpu){  add_a_helper(cpu, cpu->HL.hi);}
static inline void add_a_l(CPU *cpu){  add_a_helper(cpu, cpu->HL.lo);}
static inline void add_a_a(CPU *cpu){  add_a_helper(cpu, cpu->AF.hi);}
static inline void add_a_m(CPU *cpu){  add_a_helper(cpu, memory_read_8(cpu->p_memory,cpu->HL.val));}

// and operation
static inline void and_b(CPU *cpu){  and_helper(cpu, cpu->BC.hi);}
static inline void and_c(CPU *cpu){  and_helper(cpu, cpu->BC.lo);}
static inline void and_d(CPU *cpu){  and_helper(cpu, cpu->DE.hi);}
static inline void and_e(CPU *cpu){  and_helper(cpu, cpu->DE.lo);}
static inline void and_h(CPU *cpu){  and_helper(cpu, cpu->HL.hi);}
static inline void and_l(CPU *cpu){  and_helper(cpu, cpu->HL.lo);}
static inline void and_a(CPU *cpu){  and_helper(cpu, cpu->AF.hi);}
static inline void and_m(CPU *cpu){  and_helper(cpu, memory_read_8(cpu->p_memory,cpu->HL.val));}


// or operation
static inline void or_b(CPU *cpu){ or_helper(cpu, cpu->BC.hi);}
static inline void or_c(CPU *cpu){ or_helper(cpu, cpu->BC.lo);}
static inline void or_d(CPU *cpu){ or_helper(cpu, cpu->DE.hi);}
static inline void or_e(CPU *cpu){ or_helper(cpu, cpu->DE.lo);}
static inline void or_h(CPU *cpu){ or_helper(cpu, cpu->HL.hi);}
static inline void or_l(CPU *cpu){ or_helper(cpu, cpu->HL.lo);}
static inline void or_a(CPU *cpu){ or_helper(cpu, cpu->AF.hi);}
static inline void or_m(CPU *cpu){ or_helper(cpu, memory_read_8(cpu->p_memory,cpu->HL.val));}

// adc operation
static inline void adc_b(CPU *cpu){ adc_helper(cpu, cpu->BC.hi);}
static inline void adc_c(CPU *cpu){ adc_helper(cpu, cpu->BC.lo);}
static inline void adc_d(CPU *cpu){ adc_helper(cpu, cpu->DE.hi);}
static inline void adc_e(CPU *cpu){ adc_helper(cpu, cpu->DE.lo);}
static inline void adc_h(CPU *cpu){ adc_helper(cpu, cpu->HL.hi);}
static inline void adc_l(CPU *cpu){ adc_helper(cpu, cpu->HL.lo);}
static inline void adc_a(CPU *cpu){ adc_helper(cpu, cpu->AF.hi);}
static inline void adc_m(CPU *cpu){ adc_helper(cpu, memory_read_8(cpu->p_memory,cpu->HL.val));}

// sbc operation
static inline void sbc_b(CPU *cpu){ sbc_helper(cpu, cpu->BC.hi);}
static inline void sbc_c(CPU *cpu){ sbc_helper(cpu, cpu->BC.lo);}
static inline void sbc_d(CPU *cpu){ sbc_helper(cpu, cpu->DE.hi);}
static inline void sbc_e(CPU *cpu){ sbc_helper(cpu, cpu->DE.lo);}
static inline void sbc_h(CPU *cpu){ sbc_helper(cpu, cpu->HL.hi);}
static inline void sbc_l(CPU *cpu){ sbc_helper(cpu, cpu->HL.lo);}
static inline void sbc_a(CPU *cpu){ sbc_helper(cpu, cpu->AF.hi);}
static inline void sbc_m(CPU *cpu){ sbc_helper(cpu, memory_read_8(cpu->p_memory,cpu->HL.val));}

// xor operation
static inline void xor_b(CPU *cpu){ xor_helper(cpu, cpu->BC.hi);}
static inline void xor_c(CPU *cpu){ xor_helper(cpu, cpu->BC.lo);}
static inline void xor_d(CPU *cpu){ xor_helper(cpu, cpu->DE.hi);}
static inline void xor_e(CPU *cpu){ xor_helper(cpu, cpu->DE.lo);}
static inline void xor_h(CPU *cpu){ xor_helper(cpu, cpu->HL.hi);}
static inline void xor_l(CPU *cpu){ xor_helper(cpu, cpu->HL.lo);}
static inline void xor_a(CPU *cpu){ xor_helper(cpu, cpu->AF.hi);}
static inline void xor_m(CPU *cpu){ xor_helper(cpu, memory_read_8(cpu->p_memory,cpu->HL.val));}

// CP operation 
static inline void cp_b(CPU *cpu){ cp_helper(cpu, cpu->BC.hi);}
static inline void cp_c(CPU *cpu){ cp_helper(cpu, cpu->BC.lo);}
static inline void cp_d(CPU *cpu){ cp_helper(cpu, cpu->DE.hi);}
static inline void cp_e(CPU *cpu){ cp_helper(cpu, cpu->DE.lo);}
static inline void cp_h(CPU *cpu){ cp_helper(cpu, cpu->HL.hi);}
static inline void cp_l(CPU *cpu){ cp_helper(cpu, cpu->HL.lo);}
static inline void cp_a(CPU *cpu){ cp_helper(cpu, cpu->AF.hi);}
static inline void cp_m(CPU *cpu){ cp_helper(cpu, memory_read_8(cpu->p_memory,cpu->HL.val));}

static inline void add_a_d8(CPU *cpu){ add_a_helper(cpu, get_next_8(cpu));}
static inline void sub_d8(CPU *cpu){ sub_helper(cpu, get_next_8(cpu));}
static inline void and_d8(CPU *cpu){ and_helper(cpu, get_next_8(cpu));}
static inline void or_d8(CPU *cpu){ or_helper(cpu, get_next_8(cpu));}
static inline void adc_a_d8(CPU *cpu){ adc_helper(cpu, get_next_8(cpu));}
static inline void sbc_d8(CPU *cpu){ sbc_helper(cpu, get_next_8(cpu));}
static inline void xor_d8(CPU *cpu){ xor_helper(cpu, get_next_8(cpu));}
static inline void cp_d8(CPU *cpu){ cp_helper(cpu, get_next_8(cpu));}

static inline void dec_c(CPU *cpu){ dec_helper(cpu,&cpu->BC.lo);}
static inline void dec_e(CPU *cpu){ dec_helper(cpu,&cpu->DE.lo);}
static inline void dec_l(CPU *cpu){ dec_helper(cpu,&cpu->HL.lo);}
static inline void dec_a(CPU *cpu){ dec_helper(cpu,&cpu->AF.hi);}
static inline void dec_b(CPU *cpu){ dec_helper(cpu,&cpu->BC.hi);}
static inline void dec_d(CPU *cpu){ dec_helper(cpu,&cpu->DE.hi);}
static inline void dec_m(CPU *cpu){ dec_helper(cpu,get_address(cpu->p_memory,cpu->HL.val));}
static inline void dec_h(CPU *cpu){ dec_helper(cpu,&cpu->HL.hi);}

// stack operations
static inline void push_bc(CPU *cpu){
    push(cpu, cpu->BC.hi);
    push(cpu, cpu->BC.lo);
}

static inline void push_de(CPU *cpu){
    push(cpu, cpu->DE.hi);
    push(cpu, cpu->DE.lo);
}

static inline void push_hl(CPU *cpu){
    push(cpu, cpu->HL.hi);
    push(cpu, cpu->HL.lo);
}

static inline void push_af(CPU *cpu){
    push(cpu, cpu->AF.hi);
    push(cpu, cpu->AF.lo &  0xF0);
}

static inline void pop_bc(CPU *cpu){
    cpu->BC.lo = pop(cpu);
    cpu->BC.hi = pop(cpu);
}

static inline void pop_de(CPU *cpu){
    cpu->DE.lo = pop(cpu);
    cpu->DE.hi = pop(cpu);
}

static inline void pop_hl(CPU *cpu){
    cpu->HL.lo = pop(cpu);
    cpu->HL.hi = pop(cpu);
}

static inline void pop_af(CPU *cpu){
    cpu->AF.lo = pop(cpu);
    cpu->AF.hi = pop(cpu);
    cpu->AF.lo &= 0xF0;
}

// INC INstructions (Rp)
static inline void inc_bc(CPU *cpu){ cpu->BC.val += 1;}
static inline void inc_de(CPU *cpu){ cpu->DE.val += 1;}
static inline void inc_hl(CPU *cpu){ cpu->HL.val += 1;}
static inline void inc_sp(CPU *cpu){ cpu->SP.val += 1;}
static inline void dec_bc(CPU *cpu){ cpu->BC.val -= 1;}
static inline void dec_de(CPU *cpu){ cpu->DE.val -= 1;}
static inline void dec_hl(CPU *cpu){ cpu->HL.val -= 1;}
static inline void dec_sp(CPU *cpu){ cpu->SP.val -= 1;}

static inline void rrca(CPU *cpu){
    u8 val = cpu->AF.hi;
    u8 bit0 = val & 0x01;

    cpu->AF.hi = (val >> 1) | (bit0 << 7);

    if (bit0)
        set_flag(cpu, CARRY);
    else
        unset_flag(cpu, CARRY);

    unset_flag(cpu, ZERO);        
    unset_flag(cpu, SUBTRACT);
    unset_flag(cpu, HALF_CARRY);
}

static inline void rra(CPU *cpu){
    u8 val = cpu->AF.hi;
    u8 old_carry = flag_c(cpu) ? 1 : 0;
    u8 bit0 = val & 0x01;

    cpu->AF.hi = (val >> 1) | (old_carry << 7);

    if (bit0)
        set_flag(cpu, CARRY);
    else
        unset_flag(cpu, CARRY);

    unset_flag(cpu, ZERO);       
    unset_flag(cpu, SUBTRACT);
    unset_flag(cpu, HALF_CARRY);
}

/* CB Prefixed -> Helper Functions */

static inline void srl_helper(CPU *cpu, u8 *reg){
    u8 val = *reg;
    u8 ans = val >> 1;
    
    // flag cy set to the  bit 0
    if ((val & 0x01))
    set_flag(cpu,CARRY);
    else
    unset_flag(cpu,CARRY);

    *reg = ans;

    if (ans == 0)
    set_flag(cpu,ZERO);
    else
    unset_flag(cpu,ZERO);

    unset_flag(cpu,HALF_CARRY);
    unset_flag(cpu,SUBTRACT);
}

static inline void rr_helper(CPU *cpu, u8 *reg){
    u8 val = *reg;
    u8 old_carry = flag_c(cpu) ? 1 : 0;
    u8 ans;


    if (val & 0x01)
        set_flag(cpu, CARRY);
    else
        unset_flag(cpu, CARRY);

    ans = (val >> 1) | (old_carry << 7);
    *reg = ans;

    if (ans == 0)
        set_flag(cpu, ZERO);
    else
        unset_flag(cpu, ZERO);

    unset_flag(cpu, SUBTRACT);
    unset_flag(cpu, HALF_CARRY);
}

static inline void bit_helper(CPU *cpu, u8 value, u8 bit){
    if ((value & (1 << bit)) == 0)
        set_flag(cpu, ZERO);
    else
        unset_flag(cpu, ZERO);

    unset_flag(cpu, SUBTRACT);
    set_flag(cpu, HALF_CARRY);
}

static inline void res_helper(u8 *value, u8 bit){
    *value &= ~(1 << bit);
}

static inline void setbit_helper(u8 *value, u8 bit){
    *value |= (1 << bit);
}

static inline void ei(CPU *cpu){cpu->schedule_ei=2;}

/* Opcode Functions Similar to Non prefixed ones */
static inline void srl_b(CPU *cpu){ srl_helper(cpu, &cpu->BC.hi);}
static inline void srl_c(CPU *cpu){ srl_helper(cpu, &cpu->BC.lo);}
static inline void srl_d(CPU *cpu){ srl_helper(cpu, &cpu->DE.hi);}
static inline void srl_e(CPU *cpu){ srl_helper(cpu, &cpu->DE.lo);}
static inline void srl_h(CPU *cpu){ srl_helper(cpu, &cpu->HL.hi);}
static inline void srl_l(CPU *cpu){ srl_helper(cpu, &cpu->HL.lo);}
static inline void srl_a(CPU *cpu){ srl_helper(cpu, &cpu->AF.hi);}
static inline void srl_m(CPU *cpu){ srl_helper(cpu, get_address(cpu->p_memory, cpu->HL.val));}

// rr
static inline void rr_b(CPU *cpu){ rr_helper(cpu, &cpu->BC.hi);}
static inline void rr_c(CPU *cpu){ rr_helper(cpu, &cpu->BC.lo);}
static inline void rr_d(CPU *cpu){ rr_helper(cpu, &cpu->DE.hi);}
static inline void rr_e(CPU *cpu){ rr_helper(cpu, &cpu->DE.lo);}
static inline void rr_h(CPU *cpu){ rr_helper(cpu, &cpu->HL.hi);}
static inline void rr_l(CPU *cpu){ rr_helper(cpu, &cpu->HL.lo);}
static inline void rr_a(CPU *cpu){ rr_helper(cpu, &cpu->AF.hi);}
static inline void rr_m(CPU *cpu){ rr_helper(cpu, get_address(cpu->p_memory, cpu->HL.val));}

// bit 
static inline void bit_0_b(CPU *cpu){ bit_helper(cpu, cpu->BC.hi, 0); }
static inline void bit_0_c(CPU *cpu){ bit_helper(cpu, cpu->BC.lo, 0); }
static inline void bit_0_d(CPU *cpu){ bit_helper(cpu, cpu->DE.hi, 0); }
static inline void bit_0_e(CPU *cpu){ bit_helper(cpu, cpu->DE.lo, 0); }
static inline void bit_0_h(CPU *cpu){ bit_helper(cpu, cpu->HL.hi, 0); }
static inline void bit_0_l(CPU *cpu){ bit_helper(cpu, cpu->HL.lo, 0); }
static inline void bit_0_a(CPU *cpu){ bit_helper(cpu, cpu->AF.hi, 0); }
static inline void bit_0_hl(CPU *cpu){
    u8 val = memory_read_8(cpu->p_memory, cpu->HL.val);
    bit_helper(cpu, val, 0);
}

static inline void bit_1_b(CPU *cpu){ bit_helper(cpu, cpu->BC.hi, 1); }
static inline void bit_1_c(CPU *cpu){ bit_helper(cpu, cpu->BC.lo, 1); }
static inline void bit_1_d(CPU *cpu){ bit_helper(cpu, cpu->DE.hi, 1); }
static inline void bit_1_e(CPU *cpu){ bit_helper(cpu, cpu->DE.lo, 1); }
static inline void bit_1_h(CPU *cpu){ bit_helper(cpu, cpu->HL.hi, 1); }
static inline void bit_1_l(CPU *cpu){ bit_helper(cpu, cpu->HL.lo, 1); }
static inline void bit_1_a(CPU *cpu){ bit_helper(cpu, cpu->AF.hi, 1); }

static inline void bit_1_hl(CPU *cpu){
    u8 val = memory_read_8(cpu->p_memory, cpu->HL.val);
    bit_helper(cpu, val, 1);
}

static inline void bit_2_b(CPU *cpu){ bit_helper(cpu, cpu->BC.hi, 2); }
static inline void bit_2_c(CPU *cpu){ bit_helper(cpu, cpu->BC.lo, 2); }
static inline void bit_2_d(CPU *cpu){ bit_helper(cpu, cpu->DE.hi, 2); }
static inline void bit_2_e(CPU *cpu){ bit_helper(cpu, cpu->DE.lo, 2); }
static inline void bit_2_h(CPU *cpu){ bit_helper(cpu, cpu->HL.hi, 2); }
static inline void bit_2_l(CPU *cpu){ bit_helper(cpu, cpu->HL.lo, 2); }
static inline void bit_2_a(CPU *cpu){ bit_helper(cpu, cpu->AF.hi, 2); }

static inline void bit_2_hl(CPU *cpu){
    u8 val = memory_read_8(cpu->p_memory, cpu->HL.val);
    bit_helper(cpu, val, 2);
}

static inline void bit_3_b(CPU *cpu){ bit_helper(cpu, cpu->BC.hi, 3); }
static inline void bit_3_c(CPU *cpu){ bit_helper(cpu, cpu->BC.lo, 3); }
static inline void bit_3_d(CPU *cpu){ bit_helper(cpu, cpu->DE.hi, 3); }
static inline void bit_3_e(CPU *cpu){ bit_helper(cpu, cpu->DE.lo, 3); }
static inline void bit_3_h(CPU *cpu){ bit_helper(cpu, cpu->HL.hi, 3); }
static inline void bit_3_l(CPU *cpu){ bit_helper(cpu, cpu->HL.lo, 3); }
static inline void bit_3_a(CPU *cpu){ bit_helper(cpu, cpu->AF.hi, 3); }

static inline void bit_3_hl(CPU *cpu){
    u8 val = memory_read_8(cpu->p_memory, cpu->HL.val);
    bit_helper(cpu, val, 3);
}

static inline void bit_4_b(CPU *cpu){ bit_helper(cpu, cpu->BC.hi, 4); }
static inline void bit_4_c(CPU *cpu){ bit_helper(cpu, cpu->BC.lo, 4); }
static inline void bit_4_d(CPU *cpu){ bit_helper(cpu, cpu->DE.hi, 4); }
static inline void bit_4_e(CPU *cpu){ bit_helper(cpu, cpu->DE.lo, 4); }
static inline void bit_4_h(CPU *cpu){ bit_helper(cpu, cpu->HL.hi, 4); }
static inline void bit_4_l(CPU *cpu){ bit_helper(cpu, cpu->HL.lo, 4); }
static inline void bit_4_a(CPU *cpu){ bit_helper(cpu, cpu->AF.hi, 4); }

static inline void bit_4_hl(CPU *cpu){
    u8 val = memory_read_8(cpu->p_memory, cpu->HL.val);
    bit_helper(cpu, val, 4);
}

static inline void bit_5_b(CPU *cpu){ bit_helper(cpu, cpu->BC.hi, 5); }
static inline void bit_5_c(CPU *cpu){ bit_helper(cpu, cpu->BC.lo, 5); }
static inline void bit_5_d(CPU *cpu){ bit_helper(cpu, cpu->DE.hi, 5); }
static inline void bit_5_e(CPU *cpu){ bit_helper(cpu, cpu->DE.lo, 5); }
static inline void bit_5_h(CPU *cpu){ bit_helper(cpu, cpu->HL.hi, 5); }
static inline void bit_5_l(CPU *cpu){ bit_helper(cpu, cpu->HL.lo, 5); }
static inline void bit_5_a(CPU *cpu){ bit_helper(cpu, cpu->AF.hi, 5); }

static inline void bit_5_hl(CPU *cpu){
    u8 val = memory_read_8(cpu->p_memory, cpu->HL.val);
    bit_helper(cpu, val, 5);
}

static inline void bit_6_b(CPU *cpu){ bit_helper(cpu, cpu->BC.hi, 6); }
static inline void bit_6_c(CPU *cpu){ bit_helper(cpu, cpu->BC.lo, 6); }
static inline void bit_6_d(CPU *cpu){ bit_helper(cpu, cpu->DE.hi, 6); }
static inline void bit_6_e(CPU *cpu){ bit_helper(cpu, cpu->DE.lo, 6); }
static inline void bit_6_h(CPU *cpu){ bit_helper(cpu, cpu->HL.hi, 6); }
static inline void bit_6_l(CPU *cpu){ bit_helper(cpu, cpu->HL.lo, 6); }
static inline void bit_6_a(CPU *cpu){ bit_helper(cpu, cpu->AF.hi, 6); }

static inline void bit_6_hl(CPU *cpu){
    u8 val = memory_read_8(cpu->p_memory, cpu->HL.val);
    bit_helper(cpu, val, 6);
}

static inline void bit_7_b(CPU *cpu){ bit_helper(cpu, cpu->BC.hi, 7); }
static inline void bit_7_c(CPU *cpu){ bit_helper(cpu, cpu->BC.lo, 7); }
static inline void bit_7_d(CPU *cpu){ bit_helper(cpu, cpu->DE.hi, 7); }
static inline void bit_7_e(CPU *cpu){ bit_helper(cpu, cpu->DE.lo, 7); }
static inline void bit_7_h(CPU *cpu){ bit_helper(cpu, cpu->HL.hi, 7); }
static inline void bit_7_l(CPU *cpu){ bit_helper(cpu, cpu->HL.lo, 7); }
static inline void bit_7_a(CPU *cpu){ bit_helper(cpu, cpu->AF.hi, 7); }

static inline void bit_7_hl(CPU *cpu){
    u8 val = memory_read_8(cpu->p_memory, cpu->HL.val);
    bit_helper(cpu, val, 7);
}

// res
static inline void res_0_b(CPU *cpu){ res_helper(&cpu->BC.hi, 0); }
static inline void res_0_c(CPU *cpu){ res_helper(&cpu->BC.lo, 0); }
static inline void res_0_d(CPU *cpu){ res_helper(&cpu->DE.hi, 0); }
static inline void res_0_e(CPU *cpu){ res_helper(&cpu->DE.lo, 0); }
static inline void res_0_h(CPU *cpu){ res_helper(&cpu->HL.hi, 0); }
static inline void res_0_l(CPU *cpu){ res_helper(&cpu->HL.lo, 0); }
static inline void res_0_a(CPU *cpu){ res_helper(&cpu->AF.hi, 0); }

static inline void res_0_hl(CPU *cpu){
    u8 val = memory_read_8(cpu->p_memory, cpu->HL.val);
    res_helper(&val, 0);
    memory_write(cpu->p_memory, cpu->HL.val, val);
}

static inline void res_1_b(CPU *cpu){ res_helper(&cpu->BC.hi, 1); }
static inline void res_1_c(CPU *cpu){ res_helper(&cpu->BC.lo, 1); }
static inline void res_1_d(CPU *cpu){ res_helper(&cpu->DE.hi, 1); }
static inline void res_1_e(CPU *cpu){ res_helper(&cpu->DE.lo, 1); }
static inline void res_1_h(CPU *cpu){ res_helper(&cpu->HL.hi, 1); }
static inline void res_1_l(CPU *cpu){ res_helper(&cpu->HL.lo, 1); }
static inline void res_1_a(CPU *cpu){ res_helper(&cpu->AF.hi, 1); }

static inline void res_1_hl(CPU *cpu){
    u8 val = memory_read_8(cpu->p_memory, cpu->HL.val);
    res_helper(&val, 1);
    memory_write(cpu->p_memory, cpu->HL.val, val);
}

static inline void res_2_b(CPU *cpu){ res_helper(&cpu->BC.hi, 2); }
static inline void res_2_c(CPU *cpu){ res_helper(&cpu->BC.lo, 2); }
static inline void res_2_d(CPU *cpu){ res_helper(&cpu->DE.hi, 2); }
static inline void res_2_e(CPU *cpu){ res_helper(&cpu->DE.lo, 2); }
static inline void res_2_h(CPU *cpu){ res_helper(&cpu->HL.hi, 2); }
static inline void res_2_l(CPU *cpu){ res_helper(&cpu->HL.lo, 2); }
static inline void res_2_a(CPU *cpu){ res_helper(&cpu->AF.hi, 2); }

static inline void res_2_hl(CPU *cpu){
    u8 val = memory_read_8(cpu->p_memory, cpu->HL.val);
    res_helper(&val, 2);
    memory_write(cpu->p_memory, cpu->HL.val, val);
}

static inline void res_3_b(CPU *cpu){ res_helper(&cpu->BC.hi, 3); }
static inline void res_3_c(CPU *cpu){ res_helper(&cpu->BC.lo, 3); }
static inline void res_3_d(CPU *cpu){ res_helper(&cpu->DE.hi, 3); }
static inline void res_3_e(CPU *cpu){ res_helper(&cpu->DE.lo, 3); }
static inline void res_3_h(CPU *cpu){ res_helper(&cpu->HL.hi, 3); }
static inline void res_3_l(CPU *cpu){ res_helper(&cpu->HL.lo, 3); }
static inline void res_3_a(CPU *cpu){ res_helper(&cpu->AF.hi, 3); }

static inline void res_3_hl(CPU *cpu){
    u8 val = memory_read_8(cpu->p_memory, cpu->HL.val);
    res_helper(&val, 3);
    memory_write(cpu->p_memory, cpu->HL.val, val);
}

static inline void res_4_b(CPU *cpu){ res_helper(&cpu->BC.hi, 4); }
static inline void res_4_c(CPU *cpu){ res_helper(&cpu->BC.lo, 4); }
static inline void res_4_d(CPU *cpu){ res_helper(&cpu->DE.hi, 4); }
static inline void res_4_e(CPU *cpu){ res_helper(&cpu->DE.lo, 4); }
static inline void res_4_h(CPU *cpu){ res_helper(&cpu->HL.hi, 4); }
static inline void res_4_l(CPU *cpu){ res_helper(&cpu->HL.lo, 4); }
static inline void res_4_a(CPU *cpu){ res_helper(&cpu->AF.hi, 4); }

static inline void res_4_hl(CPU *cpu){
    u8 val = memory_read_8(cpu->p_memory, cpu->HL.val);
    res_helper(&val, 4);
    memory_write(cpu->p_memory, cpu->HL.val, val);
}

static inline void res_5_b(CPU *cpu){ res_helper(&cpu->BC.hi, 5); }
static inline void res_5_c(CPU *cpu){ res_helper(&cpu->BC.lo, 5); }
static inline void res_5_d(CPU *cpu){ res_helper(&cpu->DE.hi, 5); }
static inline void res_5_e(CPU *cpu){ res_helper(&cpu->DE.lo, 5); }
static inline void res_5_h(CPU *cpu){ res_helper(&cpu->HL.hi, 5); }
static inline void res_5_l(CPU *cpu){ res_helper(&cpu->HL.lo, 5); }
static inline void res_5_a(CPU *cpu){ res_helper(&cpu->AF.hi, 5); }

static inline void res_5_hl(CPU *cpu){
    u8 val = memory_read_8(cpu->p_memory, cpu->HL.val);
    res_helper(&val, 5);
    memory_write(cpu->p_memory, cpu->HL.val, val);
}

static inline void res_6_b(CPU *cpu){ res_helper(&cpu->BC.hi, 6); }
static inline void res_6_c(CPU *cpu){ res_helper(&cpu->BC.lo, 6); }
static inline void res_6_d(CPU *cpu){ res_helper(&cpu->DE.hi, 6); }
static inline void res_6_e(CPU *cpu){ res_helper(&cpu->DE.lo, 6); }
static inline void res_6_h(CPU *cpu){ res_helper(&cpu->HL.hi, 6); }
static inline void res_6_l(CPU *cpu){ res_helper(&cpu->HL.lo, 6); }
static inline void res_6_a(CPU *cpu){ res_helper(&cpu->AF.hi, 6); }

static inline void res_6_hl(CPU *cpu){
    u8 val = memory_read_8(cpu->p_memory, cpu->HL.val);
    res_helper(&val, 6);
    memory_write(cpu->p_memory, cpu->HL.val, val);
}

static inline void res_7_b(CPU *cpu){ res_helper(&cpu->BC.hi, 7); }
static inline void res_7_c(CPU *cpu){ res_helper(&cpu->BC.lo, 7); }
static inline void res_7_d(CPU *cpu){ res_helper(&cpu->DE.hi, 7); }
static inline void res_7_e(CPU *cpu){ res_helper(&cpu->DE.lo, 7); }
static inline void res_7_h(CPU *cpu){ res_helper(&cpu->HL.hi, 7); }
static inline void res_7_l(CPU *cpu){ res_helper(&cpu->HL.lo, 7); }
static inline void res_7_a(CPU *cpu){ res_helper(&cpu->AF.hi, 7); }

static inline void res_7_hl(CPU *cpu){
    u8 val = memory_read_8(cpu->p_memory, cpu->HL.val);
    res_helper(&val, 7);
    memory_write(cpu->p_memory, cpu->HL.val, val);
}

//set
static inline void set_0_b(CPU *cpu){ setbit_helper(&cpu->BC.hi, 0); }
static inline void set_0_c(CPU *cpu){ setbit_helper(&cpu->BC.lo, 0); }
static inline void set_0_d(CPU *cpu){ setbit_helper(&cpu->DE.hi, 0); }
static inline void set_0_e(CPU *cpu){ setbit_helper(&cpu->DE.lo, 0); }
static inline void set_0_h(CPU *cpu){ setbit_helper(&cpu->HL.hi, 0); }
static inline void set_0_l(CPU *cpu){ setbit_helper(&cpu->HL.lo, 0); }
static inline void set_0_a(CPU *cpu){ setbit_helper(&cpu->AF.hi, 0); }

static inline void set_0_hl(CPU *cpu){
    u8 val = memory_read_8(cpu->p_memory, cpu->HL.val);
    setbit_helper(&val, 0);
    memory_write(cpu->p_memory, cpu->HL.val, val);
}

static inline void set_1_b(CPU *cpu){ setbit_helper(&cpu->BC.hi, 1); }
static inline void set_1_c(CPU *cpu){ setbit_helper(&cpu->BC.lo, 1); }
static inline void set_1_d(CPU *cpu){ setbit_helper(&cpu->DE.hi, 1); }
static inline void set_1_e(CPU *cpu){ setbit_helper(&cpu->DE.lo, 1); }
static inline void set_1_h(CPU *cpu){ setbit_helper(&cpu->HL.hi, 1); }
static inline void set_1_l(CPU *cpu){ setbit_helper(&cpu->HL.lo, 1); }
static inline void set_1_a(CPU *cpu){ setbit_helper(&cpu->AF.hi, 1); }

static inline void set_1_hl(CPU *cpu){
    u8 val = memory_read_8(cpu->p_memory, cpu->HL.val);
    setbit_helper(&val, 1);
    memory_write(cpu->p_memory, cpu->HL.val, val);
}

static inline void set_2_b(CPU *cpu){ setbit_helper(&cpu->BC.hi, 2); }
static inline void set_2_c(CPU *cpu){ setbit_helper(&cpu->BC.lo, 2); }
static inline void set_2_d(CPU *cpu){ setbit_helper(&cpu->DE.hi, 2); }
static inline void set_2_e(CPU *cpu){ setbit_helper(&cpu->DE.lo, 2); }
static inline void set_2_h(CPU *cpu){ setbit_helper(&cpu->HL.hi, 2); }
static inline void set_2_l(CPU *cpu){ setbit_helper(&cpu->HL.lo, 2); }
static inline void set_2_a(CPU *cpu){ setbit_helper(&cpu->AF.hi, 2); }

static inline void set_2_hl(CPU *cpu){
    u8 val = memory_read_8(cpu->p_memory, cpu->HL.val);
    setbit_helper(&val, 2);
    memory_write(cpu->p_memory, cpu->HL.val, val);
}

static inline void set_3_b(CPU *cpu){ setbit_helper(&cpu->BC.hi, 3); }
static inline void set_3_c(CPU *cpu){ setbit_helper(&cpu->BC.lo, 3); }
static inline void set_3_d(CPU *cpu){ setbit_helper(&cpu->DE.hi, 3); }
static inline void set_3_e(CPU *cpu){ setbit_helper(&cpu->DE.lo, 3); }
static inline void set_3_h(CPU *cpu){ setbit_helper(&cpu->HL.hi, 3); }
static inline void set_3_l(CPU *cpu){ setbit_helper(&cpu->HL.lo, 3); }
static inline void set_3_a(CPU *cpu){ setbit_helper(&cpu->AF.hi, 3); }

static inline void set_3_hl(CPU *cpu){
    u8 val = memory_read_8(cpu->p_memory, cpu->HL.val);
    setbit_helper(&val, 3);
    memory_write(cpu->p_memory, cpu->HL.val, val);
}

static inline void set_4_b(CPU *cpu){ setbit_helper(&cpu->BC.hi, 4); }
static inline void set_4_c(CPU *cpu){ setbit_helper(&cpu->BC.lo, 4); }
static inline void set_4_d(CPU *cpu){ setbit_helper(&cpu->DE.hi, 4); }
static inline void set_4_e(CPU *cpu){ setbit_helper(&cpu->DE.lo, 4); }
static inline void set_4_h(CPU *cpu){ setbit_helper(&cpu->HL.hi, 4); }
static inline void set_4_l(CPU *cpu){ setbit_helper(&cpu->HL.lo, 4); }
static inline void set_4_a(CPU *cpu){ setbit_helper(&cpu->AF.hi, 4); }

static inline void set_4_hl(CPU *cpu){
    u8 val = memory_read_8(cpu->p_memory, cpu->HL.val);
    setbit_helper(&val, 4);
    memory_write(cpu->p_memory, cpu->HL.val, val);
}

static inline void set_5_b(CPU *cpu){ setbit_helper(&cpu->BC.hi, 5); }
static inline void set_5_c(CPU *cpu){ setbit_helper(&cpu->BC.lo, 5); }
static inline void set_5_d(CPU *cpu){ setbit_helper(&cpu->DE.hi, 5); }
static inline void set_5_e(CPU *cpu){ setbit_helper(&cpu->DE.lo, 5); }
static inline void set_5_h(CPU *cpu){ setbit_helper(&cpu->HL.hi, 5); }
static inline void set_5_l(CPU *cpu){ setbit_helper(&cpu->HL.lo, 5); }
static inline void set_5_a(CPU *cpu){ setbit_helper(&cpu->AF.hi, 5); }

static inline void set_5_hl(CPU *cpu){
    u8 val = memory_read_8(cpu->p_memory, cpu->HL.val);
    setbit_helper(&val, 5);
    memory_write(cpu->p_memory, cpu->HL.val, val);
}

static inline void set_6_b(CPU *cpu){ setbit_helper(&cpu->BC.hi, 6); }
static inline void set_6_c(CPU *cpu){ setbit_helper(&cpu->BC.lo, 6); }
static inline void set_6_d(CPU *cpu){ setbit_helper(&cpu->DE.hi, 6); }
static inline void set_6_e(CPU *cpu){ setbit_helper(&cpu->DE.lo, 6); }
static inline void set_6_h(CPU *cpu){ setbit_helper(&cpu->HL.hi, 6); }
static inline void set_6_l(CPU *cpu){ setbit_helper(&cpu->HL.lo, 6); }
static inline void set_6_a(CPU *cpu){ setbit_helper(&cpu->AF.hi, 6); }

static inline void set_6_hl(CPU *cpu){
    u8 val = memory_read_8(cpu->p_memory, cpu->HL.val);
    setbit_helper(&val, 6);
    memory_write(cpu->p_memory, cpu->HL.val, val);
}

static inline void set_7_b(CPU *cpu){ setbit_helper(&cpu->BC.hi, 7); }
static inline void set_7_c(CPU *cpu){ setbit_helper(&cpu->BC.lo, 7); }
static inline void set_7_d(CPU *cpu){ setbit_helper(&cpu->DE.hi, 7); }
static inline void set_7_e(CPU *cpu){ setbit_helper(&cpu->DE.lo, 7); }
static inline void set_7_h(CPU *cpu){ setbit_helper(&cpu->HL.hi, 7); }
static inline void set_7_l(CPU *cpu){ setbit_helper(&cpu->HL.lo, 7); }
static inline void set_7_a(CPU *cpu){ setbit_helper(&cpu->AF.hi, 7); }

static inline void set_7_hl(CPU *cpu){
    u8 val = memory_read_8(cpu->p_memory, cpu->HL.val);
    setbit_helper(&val, 7);
    memory_write(cpu->p_memory, cpu->HL.val, val);
}

static Opcode prefixed_opcodes[256]={
    [0x38] = {"SRL B", 2, &srl_b},
    [0x39] = {"SRL C", 2, &srl_c},
    [0x3A] = {"SRL D", 2, &srl_d},
    [0x3B] = {"SRL E", 2, &srl_e},
    [0x3C] = {"SRL H", 2, &srl_h},
    [0x3D] = {"SRL L", 2, &srl_l},
    [0x3E] = {"SRL (HL)", 2, &srl_m},
    [0x3F] = {"SRL A", 2, &srl_a},

    [0x18] = {"RR B", 2, &rr_b},
    [0x19] = {"RR C", 2, &rr_c},
    [0x1A] = {"RR D", 2, &rr_d},
    [0x1B] = {"RR E", 2, &rr_e},
    [0x1C] = {"RR H", 2, &rr_h},
    [0x1D] = {"RR L", 2, &rr_l},
    [0x1E] = {"RR (HL)", 2, &rr_m},
    [0x1F] = {"RR a", 2, &rr_a},

    [0x40] = {"BIT 0,B", 2, &bit_0_b},
    [0x41] = {"BIT 0,C", 2, &bit_0_c},
    [0x42] = {"BIT 0,D", 2, &bit_0_d},
    [0x43] = {"BIT 0,E", 2, &bit_0_e},
    [0x44] = {"BIT 0,H", 2, &bit_0_h},
    [0x45] = {"BIT 0,L", 2, &bit_0_l},
    [0x46] = {"BIT 0,(HL)", 3, &bit_0_hl},
    [0x47] = {"BIT 0,A", 2, &bit_0_a},

    [0x48] = {"BIT 1,B", 2, &bit_1_b},
    [0x49] = {"BIT 1,C", 2, &bit_1_c},
    [0x4A] = {"BIT 1,D", 2, &bit_1_d},
    [0x4B] = {"BIT 1,E", 2, &bit_1_e},
    [0x4C] = {"BIT 1,H", 2, &bit_1_h},
    [0x4D] = {"BIT 1,L", 2, &bit_1_l},
    [0x4E] = {"BIT 1,(HL)", 3, &bit_1_hl},
    [0x4F] = {"BIT 1,A", 2, &bit_1_a},

    [0x50] = {"BIT 2,B", 2, &bit_2_b},
    [0x51] = {"BIT 2,C", 2, &bit_2_c},
    [0x52] = {"BIT 2,D", 2, &bit_2_d},
    [0x53] = {"BIT 2,E", 2, &bit_2_e},
    [0x54] = {"BIT 2,H", 2, &bit_2_h},
    [0x55] = {"BIT 2,L", 2, &bit_2_l},
    [0x56] = {"BIT 2,(HL)", 3, &bit_2_hl},
    [0x57] = {"BIT 2,A", 2, &bit_2_a},

    [0x58] = {"BIT 3,B", 2, &bit_3_b},
    [0x59] = {"BIT 3,C", 2, &bit_3_c},
    [0x5A] = {"BIT 3,D", 2, &bit_3_d},
    [0x5B] = {"BIT 3,E", 2, &bit_3_e},
    [0x5C] = {"BIT 3,H", 2, &bit_3_h},
    [0x5D] = {"BIT 3,L", 2, &bit_3_l},
    [0x5E] = {"BIT 3,(HL)", 3, &bit_3_hl},
    [0x5F] = {"BIT 3,A", 2, &bit_3_a},

    [0x60] = {"BIT 4,B", 2, &bit_4_b},
    [0x61] = {"BIT 4,C", 2, &bit_4_c},
    [0x62] = {"BIT 4,D", 2, &bit_4_d},
    [0x63] = {"BIT 4,E", 2, &bit_4_e},
    [0x64] = {"BIT 4,H", 2, &bit_4_h},
    [0x65] = {"BIT 4,L", 2, &bit_4_l},
    [0x66] = {"BIT 4,(HL)", 3, &bit_4_hl},
    [0x67] = {"BIT 4,A", 2, &bit_4_a},

    [0x68] = {"BIT 5,B", 2, &bit_5_b},
    [0x69] = {"BIT 5,C", 2, &bit_5_c},
    [0x6A] = {"BIT 5,D", 2, &bit_5_d},
    [0x6B] = {"BIT 5,E", 2, &bit_5_e},
    [0x6C] = {"BIT 5,H", 2, &bit_5_h},
    [0x6D] = {"BIT 5,L", 2, &bit_5_l},
    [0x6E] = {"BIT 5,(HL)", 3, &bit_5_hl},
    [0x6F] = {"BIT 5,A", 2, &bit_5_a},

    [0x70] = {"BIT 6,B", 2, &bit_6_b},
    [0x71] = {"BIT 6,C", 2, &bit_6_c},
    [0x72] = {"BIT 6,D", 2, &bit_6_d},
    [0x73] = {"BIT 6,E", 2, &bit_6_e},
    [0x74] = {"BIT 6,H", 2, &bit_6_h},
    [0x75] = {"BIT 6,L", 2, &bit_6_l},
    [0x76] = {"BIT 6,(HL)", 3, &bit_6_hl},
    [0x77] = {"BIT 6,A", 2, &bit_6_a},

    [0x78] = {"BIT 7,B", 2, &bit_7_b},
    [0x79] = {"BIT 7,C", 2, &bit_7_c},
    [0x7A] = {"BIT 7,D", 2, &bit_7_d},
    [0x7B] = {"BIT 7,E", 2, &bit_7_e},
    [0x7C] = {"BIT 7,H", 2, &bit_7_h},
    [0x7D] = {"BIT 7,L", 2, &bit_7_l},
    [0x7E] = {"BIT 7,(HL)", 3, &bit_7_hl},
    [0x7F] = {"BIT 7,A", 2, &bit_7_a},

    // RES 0
    [0x80] = {"RES 0,B", 2, &res_0_b},
    [0x81] = {"RES 0,C", 2, &res_0_c},
    [0x82] = {"RES 0,D", 2, &res_0_d},
    [0x83] = {"RES 0,E", 2, &res_0_e},
    [0x84] = {"RES 0,H", 2, &res_0_h},
    [0x85] = {"RES 0,L", 2, &res_0_l},
    [0x86] = {"RES 0,(HL)", 3, &res_0_hl},
    [0x87] = {"RES 0,A", 2, &res_0_a},

    // RES 1
    [0x88] = {"RES 1,B", 2, &res_1_b},
    [0x89] = {"RES 1,C", 2, &res_1_c},
    [0x8A] = {"RES 1,D", 2, &res_1_d},
    [0x8B] = {"RES 1,E", 2, &res_1_e},
    [0x8C] = {"RES 1,H", 2, &res_1_h},
    [0x8D] = {"RES 1,L", 2, &res_1_l},
    [0x8E] = {"RES 1,(HL)", 3, &res_1_hl},
    [0x8F] = {"RES 1,A", 2, &res_1_a},

    // RES 2
    [0x90] = {"RES 2,B", 2, &res_2_b},
    [0x91] = {"RES 2,C", 2, &res_2_c},
    [0x92] = {"RES 2,D", 2, &res_2_d},
    [0x93] = {"RES 2,E", 2, &res_2_e},
    [0x94] = {"RES 2,H", 2, &res_2_h},
    [0x95] = {"RES 2,L", 2, &res_2_l},
    [0x96] = {"RES 2,(HL)", 3, &res_2_hl},
    [0x97] = {"RES 2,A", 2, &res_2_a},

    // RES 3
    [0x98] = {"RES 3,B", 2, &res_3_b},
    [0x99] = {"RES 3,C", 2, &res_3_c},
    [0x9A] = {"RES 3,D", 2, &res_3_d},
    [0x9B] = {"RES 3,E", 2, &res_3_e},
    [0x9C] = {"RES 3,H", 2, &res_3_h},
    [0x9D] = {"RES 3,L", 2, &res_3_l},
    [0x9E] = {"RES 3,(HL)", 3, &res_3_hl},
    [0x9F] = {"RES 3,A", 2, &res_3_a},

    // RES 4
    [0xA0] = {"RES 4,B", 2, &res_4_b},
    [0xA1] = {"RES 4,C", 2, &res_4_c},
    [0xA2] = {"RES 4,D", 2, &res_4_d},
    [0xA3] = {"RES 4,E", 2, &res_4_e},
    [0xA4] = {"RES 4,H", 2, &res_4_h},
    [0xA5] = {"RES 4,L", 2, &res_4_l},
    [0xA6] = {"RES 4,(HL)", 3, &res_4_hl},
    [0xA7] = {"RES 4,A", 2, &res_4_a},

    // RES 5
    [0xA8] = {"RES 5,B", 2, &res_5_b},
    [0xA9] = {"RES 5,C", 2, &res_5_c},
    [0xAA] = {"RES 5,D", 2, &res_5_d},
    [0xAB] = {"RES 5,E", 2, &res_5_e},
    [0xAC] = {"RES 5,H", 2, &res_5_h},
    [0xAD] = {"RES 5,L", 2, &res_5_l},
    [0xAE] = {"RES 5,(HL)", 3, &res_5_hl},
    [0xAF] = {"RES 5,A", 2, &res_5_a},

    // RES 6
    [0xB0] = {"RES 6,B", 2, &res_6_b},
    [0xB1] = {"RES 6,C", 2, &res_6_c},
    [0xB2] = {"RES 6,D", 2, &res_6_d},
    [0xB3] = {"RES 6,E", 2, &res_6_e},
    [0xB4] = {"RES 6,H", 2, &res_6_h},
    [0xB5] = {"RES 6,L", 2, &res_6_l},
    [0xB6] = {"RES 6,(HL)", 3, &res_6_hl},
    [0xB7] = {"RES 6,A", 2, &res_6_a},

    // RES 7
    [0xB8] = {"RES 7,B", 2, &res_7_b},
    [0xB9] = {"RES 7,C", 2, &res_7_c},
    [0xBA] = {"RES 7,D", 2, &res_7_d},
    [0xBB] = {"RES 7,E", 2, &res_7_e},
    [0xBC] = {"RES 7,H", 2, &res_7_h},
    [0xBD] = {"RES 7,L", 2, &res_7_l},
    [0xBE] = {"RES 7,(HL)", 3, &res_7_hl},
    [0xBF] = {"RES 7,A", 2, &res_7_a},

    // SET 0
    [0xC0] = {"SET 0,B", 2, &set_0_b},
    [0xC1] = {"SET 0,C", 2, &set_0_c},
    [0xC2] = {"SET 0,D", 2, &set_0_d},
    [0xC3] = {"SET 0,E", 2, &set_0_e},
    [0xC4] = {"SET 0,H", 2, &set_0_h},
    [0xC5] = {"SET 0,L", 2, &set_0_l},
    [0xC6] = {"SET 0,(HL)", 3, &set_0_hl},
    [0xC7] = {"SET 0,A", 2, &set_0_a},

    // SET 1
    [0xC8] = {"SET 1,B", 2, &set_1_b},
    [0xC9] = {"SET 1,C", 2, &set_1_c},
    [0xCA] = {"SET 1,D", 2, &set_1_d},
    [0xCB] = {"SET 1,E", 2, &set_1_e},
    [0xCC] = {"SET 1,H", 2, &set_1_h},
    [0xCD] = {"SET 1,L", 2, &set_1_l},
    [0xCE] = {"SET 1,(HL)", 3, &set_1_hl},
    [0xCF] = {"SET 1,A", 2, &set_1_a},

    // SET 2
    [0xD0] = {"SET 2,B", 2, &set_2_b},
    [0xD1] = {"SET 2,C", 2, &set_2_c},
    [0xD2] = {"SET 2,D", 2, &set_2_d},
    [0xD3] = {"SET 2,E", 2, &set_2_e},
    [0xD4] = {"SET 2,H", 2, &set_2_h},
    [0xD5] = {"SET 2,L", 2, &set_2_l},
    [0xD6] = {"SET 2,(HL)", 3, &set_2_hl},
    [0xD7] = {"SET 2,A", 2, &set_2_a},

    // SET 3
    [0xD8] = {"SET 3,B", 2, &set_3_b},
    [0xD9] = {"SET 3,C", 2, &set_3_c},
    [0xDA] = {"SET 3,D", 2, &set_3_d},
    [0xDB] = {"SET 3,E", 2, &set_3_e},
    [0xDC] = {"SET 3,H", 2, &set_3_h},
    [0xDD] = {"SET 3,L", 2, &set_3_l},
    [0xDE] = {"SET 3,(HL)", 3, &set_3_hl},
    [0xDF] = {"SET 3,A", 2, &set_3_a},

    // SET 4
    [0xE0] = {"SET 4,B", 2, &set_4_b},
    [0xE1] = {"SET 4,C", 2, &set_4_c},
    [0xE2] = {"SET 4,D", 2, &set_4_d},
    [0xE3] = {"SET 4,E", 2, &set_4_e},
    [0xE4] = {"SET 4,H", 2, &set_4_h},
    [0xE5] = {"SET 4,L", 2, &set_4_l},
    [0xE6] = {"SET 4,(HL)", 3, &set_4_hl},
    [0xE7] = {"SET 4,A", 2, &set_4_a},

    // SET 5
    [0xE8] = {"SET 5,B", 2, &set_5_b},
    [0xE9] = {"SET 5,C", 2, &set_5_c},
    [0xEA] = {"SET 5,D", 2, &set_5_d},
    [0xEB] = {"SET 5,E", 2, &set_5_e},
    [0xEC] = {"SET 5,H", 2, &set_5_h},
    [0xED] = {"SET 5,L", 2, &set_5_l},
    [0xEE] = {"SET 5,(HL)", 3, &set_5_hl},
    [0xEF] = {"SET 5,A", 2, &set_5_a},

    // SET 6
    [0xF0] = {"SET 6,B", 2, &set_6_b},
    [0xF1] = {"SET 6,C", 2, &set_6_c},
    [0xF2] = {"SET 6,D", 2, &set_6_d},
    [0xF3] = {"SET 6,E", 2, &set_6_e},
    [0xF4] = {"SET 6,H", 2, &set_6_h},
    [0xF5] = {"SET 6,L", 2, &set_6_l},
    [0xF6] = {"SET 6,(HL)", 3, &set_6_hl},
    [0xF7] = {"SET 6,A", 2, &set_6_a},

    // SET 7
    [0xF8] = {"SET 7,B", 2, &set_7_b},
    [0xF9] = {"SET 7,C", 2, &set_7_c},
    [0xFA] = {"SET 7,D", 2, &set_7_d},
    [0xFB] = {"SET 7,E", 2, &set_7_e},
    [0xFC] = {"SET 7,H", 2, &set_7_h},
    [0xFD] = {"SET 7,L", 2, &set_7_l},
    [0xFE] = {"SET 7,(HL)", 3, &set_7_hl},
    [0xFF] = {"SET 7,A", 2, &set_7_a},
};

static inline void cb_helper(CPU *cpu){
    u8 micro_ins = get_next_8(cpu);
    Opcode prefixed_opcode = prefixed_opcodes[micro_ins];

    if(prefixed_opcode.opcode_method != NULL){
        #ifdef DEBUG
            printf("[Executing Prefixed opcode: %s]\n", prefixed_opcode.name);
        #endif
        prefixed_opcode.opcode_method(cpu);
        cpu->cycles += prefixed_opcode.cycles;
    }
    else{
        
        
        printf("[NOT IMPLEMENTED PREFIXED OPCODE: %x]\n",micro_ins);
        exit(0);
    }
}

// misc instructions
static inline void cpl(CPU *cpu) { cpu->AF.hi = ~cpu->AF.hi;}
static inline void ccf(CPU *cpu)
 {  if (flag_c(cpu) == 1)
        unset_flag(cpu,CARRY);
    else 
        set_flag(cpu, CARRY);
}

static Opcode opcodes[256]= {
    [0xCB] = {"CB Prefixed", 0, &cb_helper},

    [0] = {"NOP",       4,      &nop},

    [0xc3] = {"JP a16", 4,      &jp_a16},
    [0xe9] = {"JP HL", 1,   &jp_hl},
    [0xc2] = {"JP NZ, a16", 0, &jp_nz_a16},
    [0xd2] = {"JP NC, a16", 0, &jp_nc_a16},
    [0xca] = {"JP Z, a16", 0, &jp_z_a16},
    [0xda] = {"JP C, a16", 0, &jp_c_a16},
    
    [0x20] = {"JR NZ, s8", 0, &jr_nz},
    [0x30] = {"JR NC, s8", 0, &jr_nc},
    [0x18] = {"JR, s8", 0, &jr_s8},
    [0x28] = {"JR Z, s8", 0, &jr_z},
    [0x38] = {"JR C, s8", 0, &jr_c},

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
    [0x04] = {"INC B", 1, &inc_b},
    [0x14] = {"INC D", 1, &inc_d},
    [0x24] = {"INC H", 1, &inc_h},
    [0x34] = {"INC (HL)", 3, &inc_m},

    [0x03] = {"INC BC", 2, &inc_bc},
    [0x13] = {"INC DE", 2, &inc_de},
    [0x23] = {"INC HL", 2, &inc_hl},
    [0x33] = {"INC SP", 2, &inc_sp},

    [0x0B] = {"DEC BC", 2, &dec_bc},
    [0x1B] = {"DEC DE", 2, &dec_de},
    [0x2B] = {"DEC HL", 2, &dec_hl},
    [0x3B] = {"DEC SP", 2, &dec_sp},

    [0xc9] = {"RET", 4, &ret},
    [0xc0] = {"RET NZ", 0, &ret_nz},
    [0xd0] = {"RET NC", 0, &ret_nc},
    [0xc8] = {"RET Z", 0, &ret_z},
    [0xd8] = {"RET C", 0, &ret_c},
    [0xd9] = {"RETI", 4, &reti},

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
    [0x57] = {"LD D, A", 1, &ld_d_a},
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
    [0x08] = {"LD (a16), SP", 5, &ld_a16_sp},


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

    [0xE0] = {"LD (a8), A", 3, &ld_a8_a},
    [0xF0] = {"LD A, (a8)", 3, &ld_a_a8},
    [0xEA] = {"LD (a16), A", 4, &ld_a16_a},
    [0xFA] = {"LD A, (a16)", 4, &ld_a_a16},
    [0xE2] = {"LD (m), A", 2, &ld_mc_a},
    [0xF2] = {"LD A, (m)", 2, &ld_a_mc},

    [0xf9] = {"LD SP, HL",2, &ld_sp_hl},
    [0xf8] = {"LD HL, SP + s8",3, &ld_hl_sp_s8},

    [0x80] = {"ADD B", 1, &add_a_b},
    [0x81] = {"ADD C", 1, &add_a_c},
    [0x82] = {"ADD D", 1, &add_a_d},
    [0x83] = {"ADD E", 1, &add_a_e},
    [0x84] = {"ADD H", 1, &add_a_h},
    [0x85] = {"ADD L", 1, &add_a_l},
    [0x86] = {"ADD M", 2, &add_a_m},
    [0x87] = {"ADD A", 1, &add_a_a},
    [0x88] = {"ADc B", 1, &adc_b},
    [0x89] = {"ADc C", 1, &adc_c},
    [0x8A] = {"ADc D", 1, &adc_d},
    [0x8B] = {"ADc E", 1, &adc_e},
    [0x8C] = {"ADc H", 1, &adc_h},
    [0x8D] = {"ADc L", 1, &adc_l},
    [0x8E] = {"ADc M", 2, &adc_m},
    [0x8F] = {"ADc A", 1, &adc_a},
    [0x27] = {"DAA", 1, &daa},
    [0x37] = {"SCF", 1, &scf},

    [0x09] = {"ADD HL, BC", 2, &add_hl_bc},
    [0x19] = {"ADD HL, DE", 2, &add_hl_de},
    [0x29] = {"ADD HL, HL", 2, &add_hl_hl},
    [0x39] = {"ADD HL, SP", 2, &add_hl_sp},

    [0x90] = {"SUB B", 1, &sub_b},
    [0x91] = {"SUB C", 1, &sub_c},
    [0x92] = {"SUB D", 1, &sub_d},
    [0x93] = {"SUB E", 1, &sub_e},
    [0x94] = {"SUB H", 1, &sub_h},
    [0x95] = {"SUB L", 1, &sub_l},
    [0x96] = {"SUB M", 2, &sub_m},
    [0x97] = {"SUB A", 1, &sub_a},
    [0x98] = {"Sbc B", 1, &sbc_b},
    [0x99] = {"Sbc C", 1, &sbc_c},
    [0x9A] = {"Sbc D", 1, &sbc_d},
    [0x9B] = {"Sbc E", 1, &sbc_e},
    [0x9C] = {"Sbc H", 1, &sbc_h},
    [0x9D] = {"Sbc L", 1, &sbc_l},
    [0x9E] = {"Sbc M", 2, &sbc_m},
    [0x9F] = {"Sbc A", 1, &sbc_a},

    [0xA0] = {"and B", 1, &and_b},
    [0xA1] = {"and C", 1, &and_c},
    [0xA2] = {"and D", 1, &and_d},
    [0xA3] = {"and E", 1, &and_e},
    [0xA4] = {"and H", 1, &and_h},
    [0xA5] = {"and L", 1, &and_l},
    [0xA6] = {"and M", 2, &and_m},
    [0xA7] = {"and A", 1, &and_a},
    [0xA8] = {"XOR B", 1, &xor_b},
    [0xA9] = {"XOR C", 1, &xor_c},
    [0xAA] = {"XOR D", 1, &xor_d},
    [0xAB] = {"XOR E", 1, &xor_e},
    [0xAC] = {"XOR H", 1, &xor_h},
    [0xAD] = {"XOR L", 1, &xor_l},
    [0xAE] = {"XOR M", 2, &xor_m},
    [0xAF] = {"XOR A", 1, &xor_a},

    [0xB0] = {"or B", 1, &or_b},
    [0xB1] = {"or C", 1, &or_c},
    [0xB2] = {"or D", 1, &or_d},
    [0xB3] = {"or E", 1, &or_e},
    [0xB4] = {"or H", 1, &or_h},
    [0xB5] = {"or L", 1, &or_l},
    [0xB6] = {"or M", 2, &or_m},
    [0xB7] = {"or A", 1, &or_a},
    [0xB8] = {"CP B", 1, &cp_b},
    [0xB9] = {"CP C", 1, &cp_c},
    [0xBA] = {"CP D", 1, &cp_d},
    [0xBB] = {"CP E", 1, &cp_e},
    [0xBC] = {"CP H", 1, &cp_h},
    [0xBD] = {"CP L", 1, &cp_l},
    [0xBE] = {"CP M", 2, &cp_m},
    [0xBF] = {"CP A", 1, &cp_a},

    [0xC6] = {"ADD A, d8", 2, &add_a_d8},
    [0xD6] = {"SUB A, d8", 2, &sub_d8},
    [0xE6] = {"AND A, d8", 2, &and_d8},
    [0xF6] = {"OR A, d8", 2, &or_d8},


    [0xCE] = {"ADC A, d8", 2, &adc_a_d8},
    [0xDE] = {"SBC A, d8", 2, &sbc_d8},
    [0xEE] = {"XOR A, d8", 2, &xor_d8},
    [0xFE] = {"CP A, d8", 2, &cp_d8},

    [0x05] = {"DEC B", 1, &dec_b},
    [0x15] = {"DEC D", 1, &dec_d},
    [0x25] = {"DEC H", 1, &dec_h},
    [0x35] = {"DEC (HL)", 3, &dec_m},

    [0x0D] = {"DEC C", 1, &dec_c},
    [0x1D] = {"DEC E", 1, &dec_e},
    [0x2D] = {"DEC L", 1, &dec_l},
    [0x3D] = {"DEC A", 1, &dec_a},

    [0xCD] = {"CALL a16",0, &call_a16},
    [0xC4] = {"CALL NZ, a16",0, &call_nz_a16},
    [0xD4] = {"CALL NC, a16",0, &call_nc_a16},
    [0xCC] = {"CALL Z, a16",0, &call_z_a16},
    [0xDC] = {"CALL C, a16",0, &call_c_a16},
    
    [0xc1] = {"POP BC",3, &pop_bc},
    [0xd1] = {"POP DE",3, &pop_de},
    [0xe1] = {"POP HL",3, &pop_hl},
    [0xf1] = {"POP AF",3, &pop_af},

    [0xc5] = {"PUSH BC",4, &push_bc},
    [0xd5] = {"PUSH DE",4, &push_de},
    [0xe5] = {"PUSH HL",4, &push_hl},
    [0xf5] = {"PUSH AF",4, &push_af},

    [0x0f] = {"RRCA", 1, &rrca},
    [0x1f] = {"RRA", 1 , &rra}, 
    
    [0xfb] = {"EI", 1, &ei},

    [0x2f] = {"CPL", 1, &cpl},
    [0x3f] = {"CCF", 1, &ccf}
};


// steps the CPU
int step_cpu(CPU *cpu){

    // logging
    #ifdef LOG
    char temp[128];

    int len = snprintf(
        temp, sizeof(temp),
        "A:%02X F:%02X B:%02X C:%02X D:%02X E:%02X H:%02X L:%02X "
        "SP:%04X PC:%04X PCMEM:%02X,%02X,%02X,%02X\n",
        cpu->AF.hi,
        cpu->AF.lo,
        cpu->BC.hi,
        cpu->BC.lo,
        cpu->DE.hi,
        cpu->DE.lo,
        cpu->HL.hi,
        cpu->HL.lo,
        cpu->SP.val,
        cpu->PC.val,
        memory_read_8(cpu->p_memory, cpu->PC.val),
        memory_read_8(cpu->p_memory, cpu->PC.val + 1),
        memory_read_8(cpu->p_memory, cpu->PC.val + 2),
        memory_read_8(cpu->p_memory, cpu->PC.val + 3)
    );
    
    if (cpu->log_pos + len >= LOG_BUFFER_SIZE) {
        FILE *fp = fopen("logging.txt", "a");
        fwrite(cpu->logs, 1, cpu->log_pos, fp);
        fclose(fp);
        cpu->log_pos = 0;
    }

    memcpy(cpu->logs + cpu->log_pos, temp, len);
    cpu->log_pos += len;

    #endif

    size_t prev_cycles = cpu->cycles;
    u8 opcode = memory_read_8(cpu->p_memory, cpu->PC.val);
    

    // next instructions
    cpu->PC.val += 1;
    
    // interrupt

    // execute the instruction
    Opcode to_exec = opcodes[opcode];
    if (to_exec.opcode_method == NULL){printf("NOT IMPLEMENTED %x\n",opcode); exit(1);}

    #ifdef DEBUG
        printf("[EXECUTING THE INSTRUCTION: %s]\n",to_exec.name);
    #endif

    to_exec.opcode_method(cpu);
    cpu->cycles += to_exec.cycles;

    // scheduled interrupt
    if (cpu->schedule_ei != 0){
        cpu->schedule_ei --;
        if(cpu->schedule_ei == 0) cpu->IME = 1;
    }
        

    return (u8) (cpu->cycles - prev_cycles);
}

