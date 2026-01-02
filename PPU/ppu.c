#include "ppu.h"
#include "memory.h"
#include "../platform/platform.h"
#include <stdbool.h>

#define LCDC 0xFF40
#define LY   0xFF44
#define SCX  0xFF43
#define SCY  0xFF42

#define TILE_MAP_0 0x9800
#define TILE_MAP_1 0x9C00

static inline void sync_ly(PPU *ppu) {
    memory_write(ppu->p_mem, LY, ppu->ly);
}

static inline u8 get_lcdc(PPU *ppu) {
    return memory_read_8(ppu->p_mem, LCDC);
}

/* =========================
   BACKGROUND SCANLINE RENDER
   ========================= */
static void render_bg_scanline(PPU *ppu) {
    u8 lcdc = get_lcdc(ppu);

    // BG disabled → color 0
    if (!(lcdc & 0x01)) {
        for (int x = 0; x < SCREEN_WIDTH; x++)
            ppu->frame_buffer[ppu->ly][x] = 0;
        return;
    }

    u16 tile_map_base = (lcdc & 0x08) ? TILE_MAP_1 : TILE_MAP_0;

    // latched scroll values (IMPORTANT)
    u8 scx = ppu->latched_scx;
    u8 scy = ppu->latched_scy;

    u8 bg_y    = ppu->ly + scy;
    u8 tile_y  = bg_y >> 3;
    u8 pixel_y = bg_y & 7;

    for (int x = 0; x < SCREEN_WIDTH; x++) {
        u8 bg_x    = x + scx;
        u8 tile_x = bg_x >> 3;
        u8 pixel_x = bg_x & 7;

        // tile index
        u16 map_addr = tile_map_base + tile_y * 32 + tile_x;
        u8 tile_id = memory_read_8(ppu->p_mem, map_addr);

        // tile data address
        u16 tile_addr;
        if (lcdc & 0x10) {
            // unsigned mode
            tile_addr = 0x8000 + tile_id * 16;
        } else {
            // signed mode (CORRECT)
            s8 signed_id = (s8)tile_id;
            tile_addr = 0x9000 + signed_id * 16;
        }

        tile_addr += pixel_y * 2;

        u8 lo = memory_read_8(ppu->p_mem, tile_addr);
        u8 hi = memory_read_8(ppu->p_mem, tile_addr + 1);

        u8 bit = 7 - pixel_x;
        u8 color =
            ((hi >> bit) & 1) << 1 |
            ((lo >> bit) & 1);

        ppu->frame_buffer[ppu->ly][x] = color;
    }
}

/* =========================
   PPU STEP
   ========================= */
void step_ppu(PPU *ppu, int cycles) {
    ppu->m_cycles += cycles;

    switch (ppu->mode) {

    /* -------- OAM SCAN -------- */
    case 2:
        if (ppu->m_cycles >= 20) {
            ppu->m_cycles -= 20;

            // latch scroll ONCE per scanline
            ppu->latched_scx = memory_read_8(ppu->p_mem, SCX);
            ppu->latched_scy = memory_read_8(ppu->p_mem, SCY);

            ppu->mode = 3;
        }
        break;

    /* -------- DRAWING -------- */
    case 3: {
        u8 fine_scroll = ppu->latched_scx & 7;
        u8 mode3_cycles = 43 + fine_scroll;

        if (ppu->m_cycles >= mode3_cycles) {
            ppu->m_cycles -= mode3_cycles;

            render_bg_scanline(ppu);
            ppu->mode = 0;
        }
        break;
    }

    /* -------- H-BLANK -------- */
    case 0:
        if (ppu->m_cycles >= 51) {
            ppu->m_cycles -= 51;

            ppu->ly++;
            sync_ly(ppu);

            if (ppu->ly == 144) {
                ppu->mode = 1;

                request_interrupt(ppu->ih, VBlank);
                present_framebuffer(ppu->draw_ctx, ppu->frame_buffer);
                screen_event_loop(ppu->draw_ctx);
            } else {
                ppu->mode = 2;
            }
        }
        break;

    /* -------- V-BLANK -------- */
    case 1:
        if (ppu->m_cycles >= 114) {
            ppu->m_cycles -= 114;

            ppu->ly++;
            sync_ly(ppu);

            if (ppu->ly >= 154) {
                ppu->ly = 0;
                sync_ly(ppu);
                ppu->mode = 2;
            }
        }
        break;
    }
}
