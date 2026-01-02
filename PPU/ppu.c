#include "ppu.h"
#include "memory.h"
#include "../platform/platform.h"
#include <stdbool.h>

#define LCDC 0xFF40
#define LY 0xFF44
#define DEFAULT_TILE_MAP  0x9800
#define SCX 0xFF43
#define SCY 0xFF42

static inline void  sync_ly(PPU *ppu){
    memory_write(ppu->p_mem, LY, ppu->ly);
}

static inline u8 get_lcdc(PPU *ppu){
    return memory_read_8(ppu->p_mem, LCDC);
}


void perform_fifo_steps(PPU *ppu){
    // described by 4.8.1 in Pandocs
    u8 lcdc = get_lcdc(ppu);

        if (!(lcdc & 0x01)) {
        // will render white if bg is disabled
        for (int x = 0; x < SCREEN_WIDTH; x++) {
            ppu->frame_buffer[ppu->ly][x] = 0;
        }
        return;
    }
        u16 tile_map_base = (lcdc & 0x08) ? 0x9C00 : 0x9800; // tile map selection

        // tile data selection
        u16 tile_data_base = (lcdc & 0x10) ? 0x8000 : 0x8800;
        bool signed_index = !(lcdc & 0x10);

        u8 scx = memory_read_8(ppu->p_mem, SCX);
        u8 scy = memory_read_8(ppu->p_mem, SCY);

        u8 bg_y = ppu->ly + scy;
        u8 tile_y = bg_y / 8;
        u8 pixel_y = bg_y % 8;

        for (int x = 0; x < SCREEN_WIDTH; x++) {
            u8 bg_x = x + scx;
            u8 tile_x = bg_x / 8;
            u8 pixel_x = bg_x % 8;

            // tile index
            u16 tile_map_addr = tile_map_base + tile_y * 32 + tile_x;
            u8 tile_id = memory_read_8(ppu->p_mem, tile_map_addr);

            // for signed mode
            if (signed_index) {
                tile_id = (s8)tile_id;
            }

            // tile address
            u16 tile_addr = tile_data_base + (tile_id * 16) + (pixel_y * 2);
            u8 lo = memory_read_8(ppu->p_mem, tile_addr);
            u8 hi = memory_read_8(ppu->p_mem, tile_addr + 1);

            u8 bit = 7 - pixel_x;
            u8 color =
                ((hi >> bit) & 1) << 1 |
                ((lo >> bit) & 1);

            ppu->frame_buffer[ppu->ly][x] = color;
    }
}


void step_ppu(PPU *ppu, int cycles){
    if (cycles == 0){
        cycles = 1;
    }
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
                
                perform_fifo_steps(ppu);

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
                    present_framebuffer(ppu->draw_ctx, ppu->frame_buffer);
                    screen_event_loop(ppu->draw_ctx);

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
}