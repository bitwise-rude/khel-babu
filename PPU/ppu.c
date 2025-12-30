#include "ppu.h"

// Mode duration
#define MODE2_CYCLES 80
#define MODE3_CYCLES 172
#define MODE0_CYCLES 204
#define SCANLINE_CYCLES (MODE2_CYCLES + MODE3_CYCLES + MODE0_CYCLES) 
#define VBLANK_LINES 10
#define TOTAL_LINES 154 


PPU init_ppu(Memory *mem){
    return (PPU){
        .p_memory = mem,
        .mode = 2,
    };
}

static inline void set_ly(PPU *ppu, u8 val){ ppu ->p_memory->IO[0x44] = val;}
static inline u8 get_ly(PPU *ppu){ return  ppu->p_memory->IO[0x44]; } 

static inline void increment_ly(PPU *ppu){  
    u8 ly = get_ly(ppu);
    ly++;
    if (ly >= TOTAL_LINES) ly = 0;
    set_ly(ppu, ly);
} 

void step_ppu(PPU *ppu,u8 cpu_cycles){
    ppu->dot_counter += cpu_cycles;

    while (1){
        switch (ppu->mode){
            case 2:
                // OAM Scan
                if(ppu->dot_counter >= MODE2_CYCLES)
                {
                    ppu->dot_counter -= MODE2_CYCLES;
                    ppu->mode = 3;
                } else return;
                break;

            case 3:
                // Pixel Transfer
                if (ppu->dot_counter >= MODE3_CYCLES )
                {
                    ppu -> mode = 0;
                    ppu -> dot_counter -= MODE3_CYCLES;
                }else return;
                break;
            
            case 0:
                // Hblank
                if(ppu->dot_counter >= MODE0_CYCLES)
                {
                    ppu->dot_counter -= MODE0_CYCLES;
                    increment_ly(ppu);

                    u8 ly = get_ly(ppu);

                    if(ly >= 144)
                    {
                        ppu->mode = 1; 
                        // TODO interrupt
                    }
                    else
                    {
                        ppu->mode = 2;  
                    }

                    ppu -> dot_counter = 0;
                }else return;
                break;
            
            case 1:
                // Vblank
                if ( ppu -> dot_counter >= SCANLINE_CYCLES){
                    ppu -> dot_counter -= SCANLINE_CYCLES;
                    increment_ly(ppu);

                    if (get_ly(ppu) == 0){
                        ppu->mode =2 ;
                        return;
                    }
                }else return;
                break;
            
            default:
                fprintf(stderr,"PPU should not be in this mode");
                exit(1);

        }
    }
}