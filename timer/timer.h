#include "../processor/cpu.h"
#include "../memory/memory.h"
#include "../interrupts/interrupts.h"

#define DIV   0xFF04
#define TIMA  0xFF05
#define TMA   0xFF06
#define TAC   0xFF07

typedef struct {
    CPU *cpu;
    int div_counter;
    InterruptManager *ih;
} Timer;

static Timer make_timer(CPU *cpu, InterruptManager *ih){
    return (Timer){
        .cpu = cpu,
        .ih = ih,
    };
}

static inline u8 mem_read(Timer *t, u16 addr){
    return memory_read_8(t->cpu->p_memory, addr);
}

static inline u8 *mem_ptr(Timer *t, u16 addr){
    return get_address(t->cpu->p_memory, addr);
}

static inline void inc_mem(Timer *t, u16 addr){
    u8 *val =  get_address(t->cpu->p_memory, addr);
    (*val) ++;
}


static void timer_step(Timer *t, int cycles)
{
    // Div counting
    t->div_counter += cycles;

    while (t->div_counter >= 64){
        t->div_counter -= 64;
        inc_mem(t,DIV);
    }
    
    // tima  and TAC
    
}
