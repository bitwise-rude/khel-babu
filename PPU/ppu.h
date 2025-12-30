/*

    The Pixel Processing Unit

*/
#pragma once
#include "../platform/platform.h"
#include "../memory/memory.h"
#include "../interrupts/interrupts.h"
#include <string.h>


typedef struct 
{
    Memory *p_memory ;
    u8 mode;
    size_t dot_counter;
    InterruptHandler *ih;
    u8 frame_buffer[144][160];
}PPU;


PPU init_ppu(Memory*, InterruptHandler *);
void step_ppu(PPU *,u8);
