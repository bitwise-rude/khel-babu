

#include "interrupts.h"
#include "memory.h"
#include "../processor/cpu.h"
#include <stdbool.h>

#define IE 0xFFFF
#define IF 0xFF0F

u16 IVT[] = {
    [VBlank] = 0x0040,
    [LCD] = 0x0048,
    [Timer] = 0x0050,
    [Serial] = 0x0058,
    [Joypad] = 0x0060,
};

InterruptManager make_interrupt_manager(CPU *cpu){
    return (InterruptManager){
        .cpu = cpu,
    };
}


void request_interrupt(InterruptManager *im, INTERRUPTS _int){
    u8 prev = memory_read_8(im->cpu->p_memory, IF);
    prev |= (1 << _int);
    memory_write(im->cpu->p_memory,IF,prev);
    // printf("WROTE REQ INTERRUPT %d\n",_int);
}

int handle_interrupt(InterruptManager *im){
    u8 ie = memory_read_8(im->cpu->p_memory,IE);
    u8 _if = memory_read_8(im -> cpu->p_memory, IF);

    if ((ie & _if) && im->cpu->is_halted){
        im->cpu->is_halted = false;
    }

    if (!im->cpu->IME)
        return 0 ;

    // for all interrupts
    for(int i=0; i<=4; i++){
        if (((ie>>i) & (0b1)) == 1) {
            // means the corresponding interrupt is enabled
            if (((_if>>i) & (0b1)) == 1){
                // means being requested
                im->cpu->IME = 0; // reset the ime 

                 // reset the corresponding IF 
                 u8 new_if = memory_read_8(im->cpu->p_memory, IF);
                new_if &= ~(1 << i);
                memory_write(im->cpu->p_memory, IF, new_if);
                
                // rst is the same thing  as calling
                rst_helper(im->cpu, IVT[i]);
                // printf("INTERRUPT CALLING\n");
                return 5;
            }

        }
    }
    return 0;
}