/*
    Interrupts (GAmEbOy)
*/
#pragma once

#include "../platform/platform.h"
#include "../processor/cpu.h"



typedef enum {
    INT_VBLANK = 0,
    INT_LCD    = 1,
    INT_TIMER  = 2,
    INT_SERIAL = 3,
    INT_JOYPAD = 4,
} INT;

static const u16 interrupt_vectors[5] = {
    0x40, // VBLANK
    0x48, // LCD
    0x50, // TIMER
    0x58, // SERIAL
    0x60  // JOYPAD
};



typedef struct {
    CPU *cpu;
}InterruptHandler;


InterruptHandler make_interrupt_handler(CPU *cpu);
void request_interrupt(InterruptHandler *ih, INT type);
void request_interrupt(InterruptHandler *ih, INT type);
void process_interrupts(InterruptHandler *ih) ;