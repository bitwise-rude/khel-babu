#include "../interrupts/interrupts.h"

#define SCREEN_WIDTH 160
#define SCREEN_HEIGHT 144

typedef struct{
    Memory *p_mem;
    u8 mode;
    int m_cycles;
    u8 ly;
    InterruptManager *ih;
}PPU;

void step_ppu(PPU *ppu, int cycles);