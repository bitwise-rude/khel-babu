#include "../processor/cpu.h"
#include "../memory/memory.h"
#include "../interrupts/interrupts.h"

#define DIV   0xFF04
#define TIMA  0xFF05
#define TMA   0xFF06
#define TAC   0xFF07

typedef struct {
    int div_cycles;     
    int tima_cycles;    
    CPU *cpu;
    InterruptHandler *ih;
} Timer;

static Timer make_timer(CPU *cpu, InterruptHandler *ih){
    return (Timer){
        .cpu = cpu,
        .ih = ih,
        .div_cycles = 0,
        .tima_cycles = 0
    };
}

static inline u8 mem_read(Timer *t, u16 addr){
    return memory_read_8(t->cpu->p_memory, addr);
}

static inline u8 *mem_ptr(Timer *t, u16 addr){
    return get_address(t->cpu->p_memory, addr);
}

static void timer_step(Timer *t, int cycles)
{

    t->div_cycles += cycles;

    while (t->div_cycles >= 64) {          
        t->div_cycles -= 64;
        (*mem_ptr(t, DIV))++;
    }

   
    u8 tac = mem_read(t, TAC);

    if (!(tac & 0x04)) {
        return; 
    }

    int period;
    switch (tac & 0x03) {
        case 0x00: period = 256; break; 
        case 0x01: period = 4;   break;
        case 0x02: period = 16;  break; 
        case 0x03: period = 64;  break;
    }

    t->tima_cycles += cycles;

    while (t->tima_cycles >= period) {
        t->tima_cycles -= period;

        u8 *tima = mem_ptr(t, TIMA);
        u8 old = *tima;
        (*tima)++;

        if (old == 0xFF) {
            *tima = mem_read(t, TMA);
            request_interrupt(t->ih, INT_TIMER);
        }
    }
}
