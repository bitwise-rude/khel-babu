#include "cpu.h"

/* Helper Macros */
#define low(a) (u8)(a & 0x00FF) 
#define high(a) (u8) ((a & 0xFF00) >> 8)

#define ZERO 0x80
#define SUBTRACT 0x40
#define HALF_CARRY 0x20
#define CARRY 0x10

#define DEBUG



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
        .SP.val = 	0xFFFE

    };
}

/* Helper functions */
// combines two bytes into a 16 bit word
static inline u16 combine_bytes(u8 hi, u8 lo)
{
    return ((u16)hi << 8) | lo;
}


static inline void set_flag(CPU *cpu, const u8 flag){
    cpu->AF.lo |= flag;
}

static inline void unset_flag(CPU *cpu, const u8 flag){
    cpu->AF.lo &= ~(flag);
}

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

static inline void dec_helper(CPU *cpu, u8 *reg){

    u8 prev = *reg;
    u8 result = prev - 1;

    *reg = result;

    // Z flag
    if (result == 0)
        set_flag(cpu, ZERO);
    else
        unset_flag(cpu, ZERO);

    // N flag (subtract)
    set_flag(cpu, SUBTRACT);

    // H flag (borrow from bit 4)
    if ((prev & 0x0F) == 0x00)
        set_flag(cpu, HALF_CARRY);
    else
        unset_flag(cpu, HALF_CARRY);

    // C flag: unaffected
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

static inline void call_helper(CPU *cpu){
    u16 addr = get_next_16(cpu);   


    push(cpu, cpu->PC.lo);
    push(cpu, cpu->PC.hi);

    cpu->PC.val = addr;
    cpu->cycles += 6;
}





static inline void rst_helper(CPU *cpu, u16 addr){
    u16 pc = cpu->PC.val;      

    push(cpu, pc & 0xFF);     
    push(cpu, pc >> 8);       

    cpu->PC.val = addr;       

}


static inline void ld_r_r_helper(u8 *dst, u8 *src){
    
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



static inline void sub_helper(CPU *cpu, const u8 operand){
    u8 prev = cpu->AF.hi;
    u8 new_value = prev - operand;

    cpu->AF.hi = new_value;

    //zero flag
    (new_value == 0) ? set_flag(cpu, ZERO): unset_flag(cpu, ZERO);

    // sub flag
    set_flag(cpu,SUBTRACT);

    // HALF-CARRY flag (borrow from bit 4)
    if ((prev & 0x0F) < (operand & 0x0F))
        set_flag(cpu, HALF_CARRY);
    else
        unset_flag(cpu, HALF_CARRY);

    // CARRY flag (borrow from bit 8)
    if (prev < operand)
        set_flag(cpu, CARRY);
    else
        unset_flag(cpu, CARRY);
}


static inline void add_a_helper(CPU *cpu,  const u8 operand){
    u8 prev = cpu->AF.hi;
    u8 new_value = prev + operand;

    cpu->AF.hi = new_value;

    //zero flag
    (new_value == 0) ? set_flag(cpu, ZERO): unset_flag(cpu, ZERO);

    // sub flag
    unset_flag(cpu,SUBTRACT);

    if (((prev & 0x0F) + (operand & 0x0F)) > 0x0F)
        set_flag(cpu, HALF_CARRY);
    else
        unset_flag(cpu, HALF_CARRY);

    // CARRY flag (carry from bit 7)
    if ((u16)prev + (u16)operand > 0xFF)
        set_flag(cpu, CARRY);
    else
        unset_flag(cpu, CARRY);
}


static inline void and_helper(CPU *cpu , const u8 operand){
    u8 result = cpu->AF.hi & operand;

    // z flag
    (result == 0) ? set_flag(cpu, ZERO): unset_flag(cpu, ZERO);

    // sub
    unset_flag(cpu,SUBTRACT);

    // half carry
    set_flag (cpu, HALF_CARRY); // documented behaviour but not sure TODO

    // carry flag
    unset_flag(cpu, CARRY);

    cpu->AF.hi = result;

}

static inline void or_helper(CPU *cpu, const u8 operand){
    u8 result = cpu->AF.hi | operand;

    // z flag
    (result == 0) ? set_flag(cpu, ZERO): unset_flag(cpu, ZERO);

    // sub flag
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

    // ZERO flag
    (new_value == 0) ? set_flag(cpu, ZERO) : unset_flag(cpu, ZERO);

    // SUB flag
    unset_flag(cpu, SUBTRACT);

    // HALF-CARRY (bit 3 -> bit 4)
    if (((prev & 0x0F) + (operand & 0x0F) + carry_in) > 0x0F)
        set_flag(cpu, HALF_CARRY);
    else
        unset_flag(cpu, HALF_CARRY);

    // CARRY (bit 7 -> bit 8)
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

    // ZERO flag
    (new_value == 0) ? set_flag(cpu, ZERO) : unset_flag(cpu, ZERO);

    // SUB flag (always set)
    set_flag(cpu, SUBTRACT);

    // HALF-CARRY flag (borrow from bit 4)
    if ((prev & 0x0F) < ((operand & 0x0F) + carry_in))
        set_flag(cpu, HALF_CARRY);
    else
        unset_flag(cpu, HALF_CARRY);

    // CARRY flag (borrow from bit 8)
    if (prev < (operand + carry_in))
        set_flag(cpu, CARRY);
    else
        unset_flag(cpu, CARRY);
}

static inline void xor_helper(CPU *cpu , const u8 operand){
    u8 result = cpu->AF.hi ^ operand;

    // z flag
    if (result == 0)
        set_flag(cpu,ZERO);
    else
        unset_flag(cpu,ZERO);

    // sub
    unset_flag(cpu,SUBTRACT);

    // half carry
    unset_flag (cpu, HALF_CARRY); 

    // carry flag
    unset_flag(cpu, CARRY);

    cpu->AF.hi = result;

}

static inline void cp_helper(CPU *cpu, const u8 operand){
    u8 prev = cpu->AF.hi;
    u8 result = prev - operand;

    // ZERO flag
    (result == 0) ? set_flag(cpu, ZERO) : unset_flag(cpu, ZERO);

    // SUB flag (always set)
    set_flag(cpu, SUBTRACT);

    // HALF-CARRY flag (borrow from bit 4)
    if ((prev & 0x0F) < (operand & 0x0F))
        set_flag(cpu, HALF_CARRY);
    else
        unset_flag(cpu, HALF_CARRY);

    // CARRY flag (borrow from bit 8)
    if (prev < operand)
        set_flag(cpu, CARRY);
    else
        unset_flag(cpu, CARRY);

}




/*  Opcode Functions  */
static inline void nop(){
    // do nothing
}

static inline void jr_helper(CPU *cpu){
    s8 offset = (s8)get_next_8(cpu);
    printf("\nOFFSET:%d\n",(int)offset);
   
    cpu->PC.val += offset;
}

static inline void jp_a16( CPU *cpu){
    // Jump to immediate 16 bit address
   
    cpu -> PC.val =  get_next_16(cpu);
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
    //u8 discard = get_next_8(cpu);//
    cpu->PC.val ++;
    cpu -> cycles += 2;

}


static inline void jr_nz(CPU *cpu){
    if(!flag_z(cpu)){
        jr_helper(cpu);
        cpu->cycles += 3;
        return;
    }
    //u8 discard = get_next_8(cpu);//
    cpu->PC.val ++;
    cpu -> cycles += 2;
}

// call
static inline void call_a16(CPU *cpu){
    call_helper(cpu);
}

static inline void call_nz_a16(CPU *cpu){
    if(!flag_z(cpu)){
        call_helper(cpu);
        cpu->cycles += 6;
        return;
    }
    cpu->PC.val +=2; // for the operands
    cpu -> cycles += 3;
}

static inline void call_nc_a16(CPU *cpu){
    if(!flag_c(cpu)){
        call_helper(cpu);
        cpu->cycles += 6;
        return;
    }
    cpu->PC.val +=2; // for the operands
    cpu -> cycles += 3;
}

static inline void call_z_a16(CPU *cpu){
    if(flag_z(cpu)){
        call_helper(cpu);
        cpu->cycles += 6;
        return;
    }
    cpu->PC.val +=2; // for the operands
    cpu -> cycles += 3;
}

static inline void call_c_a16(CPU *cpu){
    if(flag_c(cpu)){
        call_helper(cpu);
        cpu->cycles += 6;
        return;
    }
    cpu->PC.val +=2; // for the operands
    cpu -> cycles += 3;
}

static inline void jr_nc(CPU *cpu){
    if(! flag_c(cpu)){
        jr_helper(cpu);
        cpu->cycles += 3;
        return;
    }
    //u8 discard = get_next_8(cpu); // TODO fix this for others too
    cpu->PC.val ++;
    cpu -> cycles += 2;

}

static inline void jr_c(CPU *cpu){
    if(flag_c(cpu)){
        jr_helper(cpu);
        cpu->cycles += 3;
        return;
    }
    //u8 discard = get_next_8(cpu); // TODO fix this for others too
    cpu->PC.val ++;
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

static inline void inc_b(CPU *cpu)
{
    u8 val = cpu->BC.hi;
    u8 res = val + 1;

    cpu->AF.lo &= ~(ZERO | SUBTRACT | HALF_CARRY);

    // Z flag
    if (res == 0)
        cpu->AF.lo |= ZERO;

    // H flag
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

    // Z flag
    if (res == 0)
        cpu->AF.lo |= ZERO;

    // H flag
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

    // Z flag
    if (res == 0)
        cpu->AF.lo |= ZERO;

    // H flag
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

    // Z flag
    if (res == 0)
        cpu->AF.lo |= ZERO;

    // H flag
    if ((val & 0x0F) == 0x0F)
        cpu->AF.lo |= HALF_CARRY;


    cpu->AF.lo &= 0xF0;

    memory_write(cpu->p_memory, cpu->HL.val, res);
}

static inline void ret(CPU *cpu){
    u8 hi = pop(cpu);
    u8 lo = pop(cpu);

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

// weird dmg ld stuff
static inline void ld_a8_a(CPU *cpu){
    u8 operand = get_next_8(cpu);

    u16 actual_adress = 0xFF00 | operand;

    memory_write(cpu->p_memory, actual_adress, cpu->AF.hi);
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

// sub operation 

static inline void sub_b(CPU *cpu){
    sub_helper(cpu, cpu->BC.hi);
}

static inline void sub_c(CPU *cpu){
    sub_helper(cpu, cpu->BC.lo);
}

static inline void sub_d(CPU *cpu){
    sub_helper(cpu, cpu->DE.hi);
}

static inline void sub_e(CPU *cpu){
    sub_helper(cpu, cpu->DE.lo);
}

static inline void sub_h(CPU *cpu){
    sub_helper(cpu, cpu->HL.hi);
}

static inline void sub_l(CPU *cpu){
    sub_helper(cpu, cpu->HL.lo);
}

static inline void sub_a(CPU *cpu){
    sub_helper(cpu, cpu->AF.hi);
}

static inline void sub_m(CPU *cpu){
    sub_helper(cpu, memory_read_8(cpu->p_memory,cpu->HL.val));
}

// add operation 

static inline void add_a_b(CPU *cpu){
    add_a_helper(cpu, cpu->BC.hi);
}

static inline void add_a_c(CPU *cpu){
    add_a_helper(cpu, cpu->BC.lo);
}

static inline void add_a_d(CPU *cpu){
    add_a_helper(cpu, cpu->DE.hi);
}

static inline void add_a_e(CPU *cpu){
    add_a_helper(cpu, cpu->DE.lo);
}

static inline void add_a_h(CPU *cpu){
    add_a_helper(cpu, cpu->HL.hi);
}

static inline void add_a_l(CPU *cpu){
    add_a_helper(cpu, cpu->HL.lo);
}

static inline void add_a_a(CPU *cpu){
    add_a_helper(cpu, cpu->AF.hi);
}

static inline void add_a_m(CPU *cpu){
    add_a_helper(cpu, memory_read_8(cpu->p_memory,cpu->HL.val));
}


// and operation 

static inline void and_b(CPU *cpu){
    and_helper(cpu, cpu->BC.hi);
}

static inline void and_c(CPU *cpu){
    and_helper(cpu, cpu->BC.lo);
}

static inline void and_d(CPU *cpu){
    and_helper(cpu, cpu->DE.hi);
}

static inline void and_e(CPU *cpu){
    and_helper(cpu, cpu->DE.lo);
}

static inline void and_h(CPU *cpu){
    and_helper(cpu, cpu->HL.hi);
}

static inline void and_l(CPU *cpu){
    and_helper(cpu, cpu->HL.lo);
}

static inline void and_a(CPU *cpu){
    and_helper(cpu, cpu->AF.hi);
}

static inline void and_m(CPU *cpu){
    and_helper(cpu, memory_read_8(cpu->p_memory,cpu->HL.val));
}


// or operation 

static inline void or_b(CPU *cpu){
    or_helper(cpu, cpu->BC.hi);
}

static inline void or_c(CPU *cpu){
    or_helper(cpu, cpu->BC.lo);
}

static inline void or_d(CPU *cpu){
    or_helper(cpu, cpu->DE.hi);
}

static inline void or_e(CPU *cpu){
    or_helper(cpu, cpu->DE.lo);
}

static inline void or_h(CPU *cpu){
    or_helper(cpu, cpu->HL.hi);
}

static inline void or_l(CPU *cpu){
    or_helper(cpu, cpu->HL.lo);
}

static inline void or_a(CPU *cpu){
    or_helper(cpu, cpu->AF.hi);
}

static inline void or_m(CPU *cpu){
    or_helper(cpu, memory_read_8(cpu->p_memory,cpu->HL.val));
}

// adc operation
static inline void adc_b(CPU *cpu){
    adc_helper(cpu, cpu->BC.hi);
}

static inline void adc_c(CPU *cpu){
    adc_helper(cpu, cpu->BC.lo);
}

static inline void adc_d(CPU *cpu){
    adc_helper(cpu, cpu->DE.hi);
}

static inline void adc_e(CPU *cpu){
    adc_helper(cpu, cpu->DE.lo);
}

static inline void adc_h(CPU *cpu){
    adc_helper(cpu, cpu->HL.hi);
}

static inline void adc_l(CPU *cpu){
    adc_helper(cpu, cpu->HL.lo);
}

static inline void adc_a(CPU *cpu){
    adc_helper(cpu, cpu->AF.hi);
}

static inline void adc_m(CPU *cpu){
    adc_helper(cpu, memory_read_8(cpu->p_memory,cpu->HL.val));
}

// sbc operation
static inline void sbc_b(CPU *cpu){
    sbc_helper(cpu, cpu->BC.hi);
}

static inline void sbc_c(CPU *cpu){
    sbc_helper(cpu, cpu->BC.lo);
}

static inline void sbc_d(CPU *cpu){
    sbc_helper(cpu, cpu->DE.hi);
}

static inline void sbc_e(CPU *cpu){
    sbc_helper(cpu, cpu->DE.lo);
}

static inline void sbc_h(CPU *cpu){
    sbc_helper(cpu, cpu->HL.hi);
}

static inline void sbc_l(CPU *cpu){
    sbc_helper(cpu, cpu->HL.lo);
}

static inline void sbc_a(CPU *cpu){
    sbc_helper(cpu, cpu->AF.hi);
}

static inline void sbc_m(CPU *cpu){
    sbc_helper(cpu, memory_read_8(cpu->p_memory,cpu->HL.val));
}

// xor operation 

static inline void xor_b(CPU *cpu){
    xor_helper(cpu, cpu->BC.hi);
}

static inline void xor_c(CPU *cpu){
    xor_helper(cpu, cpu->BC.lo);
}

static inline void xor_d(CPU *cpu){
    xor_helper(cpu, cpu->DE.hi);
}

static inline void xor_e(CPU *cpu){
    xor_helper(cpu, cpu->DE.lo);
}

static inline void xor_h(CPU *cpu){
    xor_helper(cpu, cpu->HL.hi);
}

static inline void xor_l(CPU *cpu){
    xor_helper(cpu, cpu->HL.lo);
}

static inline void xor_a(CPU *cpu){
    xor_helper(cpu, cpu->AF.hi);
}

static inline void xor_m(CPU *cpu){
    xor_helper(cpu, memory_read_8(cpu->p_memory,cpu->HL.val));
}

// xor operation 

static inline void cp_b(CPU *cpu){
    cp_helper(cpu, cpu->BC.hi);
}

static inline void cp_c(CPU *cpu){
    cp_helper(cpu, cpu->BC.lo);
}

static inline void cp_d(CPU *cpu){
    cp_helper(cpu, cpu->DE.hi);
}

static inline void cp_e(CPU *cpu){
    cp_helper(cpu, cpu->DE.lo);
}

static inline void cp_h(CPU *cpu){
    cp_helper(cpu, cpu->HL.hi);
}

static inline void cp_l(CPU *cpu){
    cp_helper(cpu, cpu->HL.lo);
}

static inline void cp_a(CPU *cpu){
    cp_helper(cpu, cpu->AF.hi);
}

static inline void cp_m(CPU *cpu){
    cp_helper(cpu, memory_read_8(cpu->p_memory,cpu->HL.val));
}

static inline void add_a_d8(CPU *cpu){
    add_a_helper(cpu, get_next_8(cpu));
}

static inline void sub_d8(CPU *cpu){
    sub_helper(cpu, get_next_8(cpu));
}

static inline void and_d8(CPU *cpu){
    and_helper(cpu, get_next_8(cpu));
}

static inline void or_d8(CPU *cpu){
    or_helper(cpu, get_next_8(cpu));
}

static inline void adc_a_d8(CPU *cpu){
    adc_helper(cpu, get_next_8(cpu));
}

static inline void sbc_d8(CPU *cpu){
    sbc_helper(cpu, get_next_8(cpu));
}

static inline void xor_d8(CPU *cpu){
    xor_helper(cpu, get_next_8(cpu));
}

static inline void cp_d8(CPU *cpu){
    cp_helper(cpu, get_next_8(cpu));
}

static inline void dec_c(CPU *cpu){
    dec_helper(cpu,&cpu->BC.lo);
}

static inline void dec_e(CPU *cpu){
    dec_helper(cpu,&cpu->DE.lo);
}

static inline void dec_l(CPU *cpu){
    dec_helper(cpu,&cpu->HL.lo);
}

static inline void dec_a(CPU *cpu){
    dec_helper(cpu,&cpu->AF.hi);
}

static inline void dec_b(CPU *cpu){
    dec_helper(cpu,&cpu->BC.hi);
}

static inline void dec_d(CPU *cpu){
    dec_helper(cpu,&cpu->DE.hi);
}

static inline void dec_m(CPU *cpu){
    // if (*src == 0xe4 && *dst == 0x1B){
        printf("FOUND\n");
        int i ;
        printf("value : %x",cpu->HL.val);
        scanf("%d",&i);
        // exit(0);
    // }
    dec_helper(cpu,get_address(cpu->p_memory,cpu->HL.val));
}

static inline void dec_h(CPU *cpu){
    dec_helper(cpu,&cpu->HL.hi);
}

// stack operations
static inline void push_bc(CPU *cpu){
    push(cpu, cpu->BC.lo);
    push(cpu, cpu->BC.hi);
}

static inline void push_de(CPU *cpu){
    push(cpu, cpu->DE.lo);
    push(cpu, cpu->DE.hi);
}

static inline void push_hl(CPU *cpu){
    push(cpu, cpu->HL.lo);
    push(cpu, cpu->HL.hi);
}

static inline void push_af(CPU *cpu){
    push(cpu, cpu->AF.lo &  0xF0);
    push(cpu, cpu->AF.hi);
}

static inline void pop_bc(CPU *cpu){
    cpu->BC.hi = pop(cpu);
    cpu->BC.lo = pop(cpu);
}

static inline void pop_de(CPU *cpu){
    cpu->DE.hi = pop(cpu);
    cpu->DE.lo = pop(cpu);
}

static inline void pop_hl(CPU *cpu){
    cpu->HL.hi = pop(cpu);
    cpu->HL.lo = pop(cpu);
}

static inline void pop_af(CPU *cpu){
 
    cpu->AF.hi = pop(cpu);
    cpu->AF.lo = pop(cpu);
    cpu->AF.lo &= 0xF0;

}

static inline void inc_bc(CPU *cpu){
    cpu->BC.val += 1;
}

static inline void inc_de(CPU *cpu){
    cpu->DE.val += 1;
}

static inline void inc_hl(CPU *cpu){
    cpu->HL.val += 1;
}

static inline void inc_sp(CPU *cpu){
    cpu->SP.val += 1;
}

static inline void dec_bc(CPU *cpu){
    cpu->BC.val -= 1;
}

static inline void dec_de(CPU *cpu){
    cpu->DE.val -= 1;
}

static inline void dec_hl(CPU *cpu){
    cpu->HL.val -= 1;
}

static inline void dec_sp(CPU *cpu){
    cpu->SP.val -= 1;
}

/* CB Prefixed One*/

// helper functions
static inline void srl_helper(CPU *cpu, u8 *reg){
    u8 val = *reg;
    u8 ans = val >> 1;
    
    // flag cy set to the  bit 0
    if ((val && 0x01) == 1)
    set_flag(cpu,CARRY);
    else
    unset_flag(cpu,CARRY);


    *reg = ans;

    // zero
    if (ans == 0)
    set_flag(cpu,ZERO);
    else
    unset_flag(cpu,ZERO);

    // other reset
    unset_flag(cpu,HALF_CARRY);
    unset_flag(cpu,SUBTRACT);
}


// opcode functions
static inline void srl_b(CPU *cpu){
    srl_helper(cpu, &cpu->BC.hi);
}

static inline void srl_c(CPU *cpu){
    srl_helper(cpu, &cpu->BC.lo);
}
static inline void srl_d(CPU *cpu){
    srl_helper(cpu, &cpu->DE.hi);
}
static inline void srl_e(CPU *cpu){
    srl_helper(cpu, &cpu->DE.lo);
}
static inline void srl_h(CPU *cpu){
    srl_helper(cpu, &cpu->HL.hi);
}
static inline void srl_l(CPU *cpu){
    srl_helper(cpu, &cpu->HL.lo);
}
static inline void srl_a(CPU *cpu){
    srl_helper(cpu, &cpu->AF.hi);
}
static inline void srl_m(CPU *cpu){
    srl_helper(cpu, get_address(cpu->p_memory, cpu->HL.val));
}

// rr
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

static inline void rr_b(CPU *cpu){
    rr_helper(cpu, &cpu->BC.hi);
}

static inline void rr_c(CPU *cpu){
    rr_helper(cpu, &cpu->BC.lo);
}

static inline void rr_d(CPU *cpu){
    rr_helper(cpu, &cpu->DE.hi);
}

static inline void rr_e(CPU *cpu){
    rr_helper(cpu, &cpu->DE.lo);
}

static inline void rr_h(CPU *cpu){
    rr_helper(cpu, &cpu->HL.hi);
}

static inline void rr_l(CPU *cpu){
    rr_helper(cpu, &cpu->HL.lo);
}

static inline void rr_a(CPU *cpu){
    rr_helper(cpu, &cpu->AF.hi);
}

static inline void rr_m(CPU *cpu){
    rr_helper(cpu, get_address(cpu->p_memory, cpu->HL.val));
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
};

static inline void cb_helper(CPU *cpu){
    printf("PREFIXED OPCODE HANDELING\n");

    u8 micro_ins = get_next_8(cpu);
    Opcode prefixed_opcode = prefixed_opcodes[micro_ins];

    if(prefixed_opcode.opcode_method != NULL){
        printf("Executing Prefixed opcode %s", prefixed_opcode.name);
        prefixed_opcode.opcode_method(cpu);
        cpu->cycles += prefixed_opcode.cycles;
    }
    else{
        printf("NOt implemnted opcode: %x\n",micro_ins);
        exit(0);
    }
}


static Opcode opcodes[256]= {
    [0xCB] = {"CB Prefixed", 0, &cb_helper},

    [0] = {"NOP",       4,      &nop},

    [0xc3] = {"JP a16", 4,      &jp_a16},
    
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
    
};


// steps the CPU
void step_cpu(CPU *cpu){
    // fetch from memory
    // int i;
    // printf("NEXT\n");
    // scanf("%d",&i);
    // printf("\n");

    // debug

        #ifdef DEBUG
    FILE *fp = fopen("logging.txt","a");
    fprintf(fp,"A:%.2x F:%.2x B:%.2x C:%.2x D:%.2x E:%.2x H:%.2x L:%.2x SP:%.4x PC:%.4x PCMEM:%.2x,%.2x,%.2x,%.2x\n",
            cpu->AF.hi,
            cpu-> AF.lo,
            cpu-> BC.hi,
            cpu->BC.lo,
            cpu->DE.hi,
            cpu->DE.lo,
            cpu->HL.hi,
            cpu->HL.lo,
            cpu->SP.val,
            cpu->PC.val,
            memory_read_8(cpu->p_memory, cpu->PC.val),
            memory_read_8(cpu->p_memory, cpu->PC.val+1),
            memory_read_8(cpu->p_memory, cpu->PC.val+2),
            memory_read_8(cpu->p_memory, cpu->PC.val+3)
        );
    
    fclose(fp);
    #endif

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

    // logging 


}