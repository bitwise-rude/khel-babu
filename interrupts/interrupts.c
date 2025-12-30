#include "interrupts.h"

InterruptHandler make_interrupt_handler(CPU *cpu){
    return (InterruptHandler) {
        .cpu = cpu
    };
}

void request_interrupt(InterruptHandler *ih, INT type){
    u8 IF = ih->cpu->p_memory->IO[0x0F];
    IF |= (1 << type); 
    ih->cpu->p_memory->IO[0x0F] = IF;   
}

void process_interrupts(InterruptHandler *ih) {
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
