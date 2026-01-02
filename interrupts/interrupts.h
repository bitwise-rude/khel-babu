#include "../processor/cpu.h"
#pragma once

typedef struct{
CPU *cpu;
}InterruptManager;

 typedef enum{
    VBlank,
    LCD,
    Timer,
    Serial,
    Joypad
}INTERRUPTS;



InterruptManager make_interrupt_manager(CPU *cpu);

int handle_interrupt(InterruptManager *im);
void request_interrupt(InterruptManager *, INTERRUPTS);