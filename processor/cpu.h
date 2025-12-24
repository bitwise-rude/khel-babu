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
    u16 AF;
    u16 BC;
    u16 DE;
    u16 HL;
    u16 PC; // program counter
    u16 SP; // stack pointer

    // memory
    Memory *p_memory;
    size_t cycles;

    //interrupts
    u8 IME;

}CPU;


 CPU init_cpu(Memory *p_mem);

typedef struct {
    char name[10];

    u8 cycles;

    void (*opcode_method)();
}Opcode;




void step_cpu(CPU *);