

#include "interrupts.h"
#include "memory.h"
#include "../processor/cpu.h"

#define IE 0xFFFF
#define IF 0xFF0F

u16 IVT[] = {
    [VBlank] = 0x0040,
    [LCD] = 0x0048,
    [Timer] = 0x0040,
    [Serial] = 0x0040,
    [Joypad] = 0x0040,
};

InterruptManager make_interrupt_manager(CPU *cpu){
    return (InterruptManager){
        .cpu = cpu,
    };
}

void handle_interrupt(InterruptManager *im){
    if(im->cpu->IME != 1) return;
    u8 ie = memory_read_8(im->cpu->p_memory,IE);
    u8 _if = memory_read_8(im -> cpu->p_memory, IF);

    // for all interrupts
    for(int i=0; i<=4; i++){
        // printf("%b\n",ie);
        if ((ie>>i) & (0b1) == 1) {
            // means the corresponding interrupt is enabled
            if ((_if>>i) & (0b1) == 1){
                printf("INT is being requested: %d\n",i);
                // means being requested
                im->cpu->IME = 0; // reset the ime 
                memory_write(im->cpu->p_memory,IF, (u8)(_if & (~(1<<i)))); // reset the corresponding IF 
                
                im->cpu->cycles += 5;

                // rst is the same thing  as calling
                rst_helper(im->cpu, IVT[i]);

            }

        }
    }
    // disable IME before calling

}