/*
    Interrupts (GAmEbOy)
*/

#include "../processor/cpu.h"


typedef enum {
    INT_VBLANK = 0,
    INT_LCD    = 1,
    INT_TIMER  = 2,
    INT_SERIAL = 3,
    INT_JOYPAD = 4,
} INT;

static const u16 interrupt_vectors[5] = {
    0x40, // VBLANK
    0x48, // LCD
    0x50, // TIMER
    0x58, // SERIAL
    0x60  // JOYPAD
};



typedef struct {
    CPU *cpu;
}InterruptHandler;

InterruptHandler make_interrupt_handler(CPU *cpu){
    return InterruptHandler {
        .cpu = cpu
    };
}

static inline void process_interrupts(InterruptHandler *ih) {
    CPU *cpu = ih->cpu;

    if (!cpu->IME)
        return;

    u8 ie = cpu->p_memory->IE;          
    u8 IF = cpu->p_memory->IO[0x0F];    // 0xFF0F

    u8 pending = ie & IF;
    if (!pending)
        return;

    for (int bit = 0; bit < 5; bit++) {
        if (pending & (1 << bit)) {

            cpu->IME = 0;                  
            cpu->p_memory->IO[0x0F] &= ~(1 << bit); 

            // push Program counter
            push(cpu, (cpu->PC.val >> 8) & 0xFF);
            push(cpu, cpu->PC.val & 0xFF);

            cpu->PC.val = interrupt_vectors[bit];
            cpu->cycles += 5; 

            return; 
        }
    }
}
