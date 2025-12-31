#include "../processor/cpu.h"
#include "../memory/memory.h"
#include "../interrupts/interrupts.h"

#define DIV     0xFF04
#define TIMA     0xFF05
#define TMA     0xFF06
#define TAC     0xFF07

typedef struct {
int div_cycles;
int tima_cycles; 
CPU *cpu;
InterruptHandler *ih;
} Timer;

static Timer make_timer(CPU *cpu, InterruptHandler *ih){
    return (Timer){
        .cpu = cpu, 
        .div_cycles = 0,
        .tima_cycles=0,
        .ih = ih,
    };
}

static inline u8 get_tac(Timer *timer){
    return memory_read_8(timer->cpu->p_memory,TAC);
}

static inline u8 get_tma(Timer *timer){
    return memory_read_8(timer->cpu->p_memory,TMA);
}

static inline void inc_tima(Timer *timer){
    u8 *prev = get_address(timer->cpu->p_memory,TIMA);
    (*prev)+=1;
}

static inline u8 get_tima(Timer *timer){
    u8 *prev = get_address(timer->cpu->p_memory,TIMA);
    return *prev;
}

static inline void set_tima(Timer *timer,u8 val){
    u8 *prev = get_address(timer->cpu->p_memory,TIMA);
    (*prev) = val;
}

static void process_time(Timer *timer, int cycles){
    timer->div_cycles += cycles;

    // for div register
    while (1){ 
        if(timer->div_cycles >= 256) {
            timer->div_cycles -= 256;
            u8 *div = get_address(timer->cpu->p_memory,DIV);
            (*div) +=1;
        }
        else{
            break;
        }
    }
    
    // for TIMA
    
    u16 tac = get_tac(timer);
    u8 enabled=  (tac & 0b00000100)>>2;
    if (enabled != 1 ){
        return;
    }
    u8 clock_select=  (tac & 0b0000011);
    int increment_every = 0;

    switch (clock_select){
        case 00:
            increment_every = 256;
            break;
        case 01:
            increment_every = 4;
            break;
        case 10:
            increment_every = 16;
            break;
        case 11:
            increment_every = 64;
            break;
        default:
            printf("[Timer Error Occured]\n");
            exit(1);
    }

    timer->tima_cycles += cycles;
    while(1){
        if(timer->tima_cycles >= increment_every){
            timer->tima_cycles -= increment_every;
            
            //increment TIMA register
            inc_tima(timer);

            if (get_tima(timer) == 0){
                // reset
                set_tima(timer, get_tma(timer));
                request_interrupt(timer->ih,(INT)2);
          
            }
        }
        else{
            return;
        }   
    }


}