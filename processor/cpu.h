/*
    CPU is an 8 bit 8080-like CPU

*/
#pragma once
#include <stdlib.h>
#include <stdint.h>
#include "../memory/memory.h"

typedef uint8_t u8;
typedef uint16_t u16;

typedef union {
    struct {
        u8 lo;  
        u8 hi;   
    };
    u16 val;
} reg16;

typedef struct
{
    
    // registers
    reg16 AF;
    reg16 BC;
    reg16 DE;
    reg16 HL;
    reg16 PC; // program counter
    reg16 SP; // stack pointer

    // memory
    Memory *p_memory;
    size_t cycles;

    //interrupts
    u8 IME;

}CPU;


 CPU init_cpu(Memory *p_mem);

typedef struct {
    char name[15];

    u8 cycles;

    void (*opcode_method)();
}Opcode;




void step_cpu(CPU *);