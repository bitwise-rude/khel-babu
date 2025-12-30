#include "../processor/cpu.h"
#include "../memory/memory.h"

static void process_time(CPU *cpu, u8 cycles){
    while (1){
        if(cycles >= 256) {
            cycles -= 256;
            // div register
            u8 *div = get_address(cpu->p_memory,0xFF04);
            *div ++;
        }
        else{
            break;
        }
    }
    
}