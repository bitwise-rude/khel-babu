#include "../interrupts/interrupts.h"
#include "../platform/platform.h"

typedef struct{
    Memory *p_mem;
    u8 mode;
    int m_cycles;
    u8 ly;
    InterruptManager *ih;
    u8 frame_buffer[SCREEN_HEIGHT][SCREEN_WIDTH];

    u8 latched_scx;
    u8 latched_scy;
    
    bool lcd_prev;
    bool stat_irq_line;

    u8 window_line;

    
    struct DrawingContext *draw_ctx;
}PPU;

void step_ppu(PPU *ppu, int cycles);