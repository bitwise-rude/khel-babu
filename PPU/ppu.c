#include "ppu.h"
#include "memory.h"

#define LY 0xFF44

static void inline sync_ly(PPU *ppu){
    memory_write(ppu->p_mem, LY, ppu->ly);
}

void step_ppu(PPU *ppu, int cycles){
    ppu->m_cycles += cycles;

    switch(ppu->mode){
        case 2:
            // OAM scan
            // TODO equals or not (less than equals to)
            if (ppu->m_cycles >= 20){
                ppu ->m_cycles -=20;
                ppu->mode = 3;
            }else return;
            break;
        case 3:
            // drawing mode
            if(ppu->m_cycles>= 43){
                ppu->m_cycles -= 43;
                ppu->mode = 0;
            }else return;
            break;
        
        case 0:
            // h-blank
            if (ppu->m_cycles >=51){
                ppu->m_cycles -= 51;

                ppu->ly ++;
                sync_ly(ppu);

                if (ppu-> ly == 144){
                    ppu->mode = 1;
                    request_interrupt(ppu->ih,VBlank);

                }else ppu->mode =  2;
                
            }else return;
            break;
            
        case 1:
        //vblank
            if (ppu -> m_cycles >= 114){
                ppu ->m_cycles -= 144;
                ppu->ly ++;
                

                if (ppu->ly >= 154){
                    ppu->ly = 0;
                    ppu->mode = 2;
                }

                sync_ly(ppu);
            }else return;
            break;
            

    }
}