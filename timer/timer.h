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
    };
}

u8 mem_read(Timer_Manager *t, u16 addr){
    return memory_read_8(t->cpu->p_memory, addr);
}

u8 *mem_ptr(Timer_Manager *t, u16 addr){
    return get_address(t->cpu->p_memory, addr);
}

void inc_mem(Timer_Manager *t, u16 addr){
    u8 *val =  get_address(t->cpu->p_memory, addr);
    (*val) ++;
}


void timer_step(Timer_Manager *t, int cycles)
{
    // 1 per T-state so, 4 per m-cycle
    t->div_counter += 4 * (cycles);

    // map top 8 bits to FF04
    u8 *div = &t->cpu->p_memory->IO[DIV-0xFF00];
    // u8 *div = get_address(t->cpu->p_memory,DIV);

    // if(*div == 0){
    //     // we know memory is written
    //     printf("MEMORY RESET");
    //     t->div_counter = 0;
    // }

    *div = (u8) (((t->div_counter) >> 8) & 0x00FF);
    
    // tac configuer
    u8 tac = mem_read(t,TAC);
    u8 t_en = (u8)((tac)>>2) &(0b00000001);
    u8 clock_sel = (u8)((tac) & (0b00000011));

    u8 test_bit;
    switch(clock_sel){
        case 0b00:
            test_bit = (t->div_counter & 0b0000001000000000) >> 9; // bit 9
            break;
        case 0b01:
            test_bit = (t->div_counter & 0b0000000000001000) >> 3; // bit 3
            break;
        case 0b10:
            test_bit = (t->div_counter & 0b0000000000100000)>> 5; // bit 5
            break;
        case 0b11:
            test_bit = (t->div_counter & 0b0000000010000000)>> 7; // bit 7
            break;
        default:
            printf("TIMER TAC this state shouldn't exist\n");
            exit(1);
    }

    u8 and_result = test_bit & t_en;

    if (t->prev_res == 1 && and_result == 0){
        inc_mem(t,TIMA);

        if (mem_read(t,TIMA) == 0){
            // overflow
            // show weird 4 t cycle wait kinda shit TODO
            u8 *tima = get_address(t->cpu->p_memory, TIMA);
            *tima = mem_read(t, TMA);

            // interrupt occurs
            printf("INTERRUPT REQUESTD BY TIMER\n");
            request_interrupt(t->ih, Timer);
            
        }
    }
    
    t->prev_res = and_result;

}
