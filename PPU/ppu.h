/*

    The Pixel Processing Unit

*/

#include "../platform/platform.h"
#include "../memory/memory.h"

typedef struct 
{
    Memory *p_memory ;
    u8 mode;
    size_t dot_counter;
}PPU;


PPU init_ppu(Memory*);
void step_ppu(PPU *,u8);