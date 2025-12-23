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

    //interrupts
    u8 IME;

}CPU;


 CPU init_cpu(Memory *p_mem);

typedef struct {
    char name[10];

    u8 t_states;

    void (*opcode_method)();
}Opcode;




void step_cpu(CPU *);