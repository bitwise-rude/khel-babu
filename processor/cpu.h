#pragma once
#include <stdlib.h>
#include <stdint.h>
#include "../memory/memory.h"
#include <stdio.h>


typedef uint8_t u8;
typedef uint16_t u16;
typedef int8_t s8;
typedef uint32_t u32;
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
    u8 schedule_ei;

    u8 stop_mode;

    // logger
    #ifdef LOG
        char logs[LOG_BUFFER_SIZE];
        size_t log_pos;
    #endif

}CPU;


 CPU init_cpu(Memory *p_mem);

typedef struct {
    char name[15];

    u8 cycles;

    void (*opcode_method)();
}Opcode;

int step_cpu(CPU *);
void push(CPU *cpu, u8 val);
