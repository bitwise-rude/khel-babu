#pragma once
#include "../processor/cpu.h"
#include "../memory/memory.h"
#include "../interrupts/interrupts.h"


#define DIV   0xFF04
#define TIMA  0xFF05
#define TMA   0xFF06
#define TAC   0xFF07

typedef struct {
    CPU *cpu;

    int div_cycles;
    u16 div_counter;
    u8 prev_res;

    InterruptManager *ih;

} Timer_Manager;

Timer_Manager make_timer(CPU *cpu, InterruptManager *ih){
    return (Timer_Manager){
        .cpu = cpu,
        .ih = ih,
        .div_cycles = 0,
        .prev_res = 0,
        .div_counter = 0,
    };
}

u8 mem_read(Timer_Manager *t, u16 addr){
    return memory_read_8(t->cpu->p_memory, addr);
}

u8 *mem_ptr(Timer_Manager *t, u16 addr){
    return get_address(t->cpu->p_memory, addr,true);
}

void inc_mem(Timer_Manager *t, u16 addr){
    u8 *val =  get_address(t->cpu->p_memory, addr,true);
    (*val) ++;
}

void timer_reset_div(Timer_Manager *t){
    // Writing any value to DIV resets the internal counter
    t->div_counter = 0;
    u8 *div = get_address(t->cpu->p_memory, DIV, true);
    *div = 0;
}


void timer_step(Timer_Manager *t, int m_cycles)
{
    for (int i = 0; i < m_cycles * 4; i++) {

        t->div_counter++;

        u8 *div = get_address(t->cpu->p_memory, DIV, false);
        *div = (t->div_counter >> 8) & 0xFF;

        u8 tac = mem_read(t, TAC);
        u8 t_en = (tac >> 2) & 1;

        u8 test_bit;
        switch (tac & 0b11) {
            case 0: test_bit = (t->div_counter >> 9) & 1; break;
            case 1: test_bit = (t->div_counter >> 3) & 1; break;
            case 2: test_bit = (t->div_counter >> 5) & 1; break;
            case 3: test_bit = (t->div_counter >> 7) & 1; break;
        }

        u8 and_result = test_bit & t_en;

        if (t->prev_res == 1 && and_result == 0) {
            u8 tima = mem_read(t, TIMA);
            if (tima == 0xFF) {
                *get_address(t->cpu->p_memory, TIMA, true) =
                    mem_read(t, TMA);
                request_interrupt(t->ih, Timer);
            } else {
                inc_mem(t, TIMA);
            }
        }

        t->prev_res = and_result;
    }
}