/*
    CPU is an 8 bit 8080-like CPU

*/
#pragma once
#include <stdlib.h>
#include <stdint.h>
#include "../memory/memory.h"

typedef uint8_t u8;
typedef uint16_t u16;


typedef struct
{
    
    // registers
    // REG16 AF;
    // REG16 BC;
    // REG16 DE;
    // REG16 HL;
    // REG16 SP;
    u16 PC;

    // memory
    Memory *p_memory;
    size_t t_states;

}CPU;

static CPU init_cpu(Memory *p_mem){
    return (CPU) {
        .PC = 0x100,
        .p_memory = p_mem
    };
}

typedef struct {
    char name[10];
    u8 code;

    u8 nobranch_tstates;
    u8 branch_tstates;

    void (*opcode_method)();
}Opcode;




void step_cpu(CPU *);