#include "ppu.h"
#include "memory.h"
#include "../platform/platform.h"

#define LCDC 0xFF40
#define LY 0xFF44

static inline void  sync_ly(PPU *ppu){
    memory_write(ppu->p_mem, LY, ppu->ly);
}

static inline u8 get_lcdc(PPU *ppu){
    return memory_read_8(ppu->p_mem, LCDC);
}

void temp_present(PPU *ppu){
    // printf("PRESENTED THE Frame Buffer\n");

    // FILE *fp = fopen("test.pgm", "wb");
    // if (!fp){
    //     perror("Failed to open test.pgm");
    //     return;
    // }

    // fprintf(fp, "P5\n160 144\n255\n");
    // fwrite(ppu->frame_buffer, 1, 160 * 144, fp);
    // fclose(fp);
    // exit(1);
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
                
                for (int i=0; i<=SCREEN_WIDTH;i++){
                    ppu->frame_buffer[ppu->ly][i] = (((ppu->ly * i)%(SCREEN_HEIGHT*SCREEN_WIDTH)))* 255;
                }

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
                    temp_present(ppu);

                }else ppu->mode =  2;
                
            }else return;
            break;
            
        case 1:
        //vblank
            if (ppu -> m_cycles >= 114){
                ppu ->m_cycles -= 114;
                ppu->ly ++;
                sync_ly(ppu);

            
                if (ppu->ly >= 154){
                    ppu->ly = 0;
                    sync_ly(ppu);
                    ppu->mode = 2;
                }

            }else return;
            break;
            

    }
    screen_event_loop(ppu->draw_ctx);
}