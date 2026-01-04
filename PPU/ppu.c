#include "ppu.h"
#include "memory.h"
#include "../platform/platform.h"
#include <stdbool.h>

#define LCDC 0xFF40
#define STAT 0xFF41
#define SCY  0xFF42
#define SCX  0xFF43
#define LY   0xFF44
#define LYC  0xFF45
#define BGP  0xFF47
#define OBP0 0xFF48
#define OBP1 0xFF49
#define WY   0xFF4A
#define WX   0xFF4B


#define OAM_BASE 0xFE00

#define TILE_MAP_0 0x9800
#define TILE_MAP_1 0x9C00

/* =========================
   HELPERS
   ========================= */

static inline u8 bg_palette(PPU *ppu, u8 c) {
    u8 p = memory_read_8(ppu->p_mem, BGP);
    return (p >> (c * 2)) & 3;
}

static inline u8 obj_palette(PPU *ppu, u8 c, bool pal1) {
    u8 p = memory_read_8(ppu->p_mem, pal1 ? OBP1 : OBP0);
    return (p >> (c * 2)) & 3;
}

static inline u8 bg_palette_lookup(PPU *ppu, u8 color) {
    u8 bgp = memory_read_8(ppu->p_mem, BGP);
    return (bgp >> (color * 2)) & 0x03;
}


static inline void sync_ly(PPU *ppu) {
    memory_write(ppu->p_mem, LY, ppu->ly);
}

static inline u8 get_lcdc(PPU *ppu) {
    return memory_read_8(ppu->p_mem, LCDC);
}

/* =========================
   STAT
   ========================= */

static void stat_update(PPU *ppu) {
    u8 stat = memory_read_8(ppu->p_mem, STAT);

    stat = (stat & ~0x03) | (ppu->mode & 3);

    u8 lyc = memory_read_8(ppu->p_mem, LYC);
    if (ppu->ly == lyc)
        stat |= (1 << 2);
    else
        stat &= ~(1 << 2);

    memory_write(ppu->p_mem, STAT, stat);
}

static void stat_check(PPU *ppu) {
    u8 stat = memory_read_8(ppu->p_mem, STAT);
    bool req = false;

    if (ppu->mode == 0 && (stat & (1 << 3))) req = true;
    if (ppu->mode == 1 && (stat & (1 << 4))) req = true;
    if (ppu->mode == 2 && (stat & (1 << 5))) req = true;
    if ((stat & (1 << 6)) && (stat & (1 << 2))) req = true;

    if (req && !ppu->stat_irq_line)
        request_interrupt(ppu->ih, LCD);

    ppu->stat_irq_line = req;
}


static void stat_check_and_request(PPU *ppu) {
    u8 stat = memory_read_8(ppu->p_mem, STAT);
    bool req = false;

    if (ppu->mode == 0 && (stat & (1 << 3))) req = true;
    if (ppu->mode == 1 && (stat & (1 << 4))) req = true;
    if (ppu->mode == 2 && (stat & (1 << 5))) req = true;
    if ((stat & (1 << 6)) && (stat & (1 << 2))) req = true;

    if (req && !ppu->stat_irq_line)
        request_interrupt(ppu->ih, LCD);

    ppu->stat_irq_line = req;
}

/* =========================
   BACKGROUND + WINDOW
   ========================= */

   static void render_bg_window(PPU *ppu, u8 bg_line[160]) {
    u8 lcdc = memory_read_8(ppu->p_mem, LCDC);

    bool bg_enable = lcdc & 1;
    bool win_enable = lcdc & (1 << 5);

    u8 scx = ppu->latched_scx;
    u8 scy = ppu->latched_scy;
    u8 wy  = memory_read_8(ppu->p_mem, WY);
    int wx = memory_read_8(ppu->p_mem, WX) - 7;

    bool window_active = false;

    for (int x = 0; x < 160; x++) {

        u8 color = 0;

        if (bg_enable) {
            bool use_win =
                win_enable &&
                ppu->ly >= wy &&
                x >= wx;

            u16 map;
            u8 px, py;

            if (use_win) {
                map = (lcdc & (1 << 6)) ? TILE_MAP_1 : TILE_MAP_0;
                px = x - wx;
                py = ppu->window_line;
                window_active = true;
            } else {
                map = (lcdc & (1 << 3)) ? TILE_MAP_1 : TILE_MAP_0;
                px = x + scx;
                py = ppu->ly + scy;
            }

            u8 tx = px >> 3;
            u8 ty = py >> 3;
            u8 fx = px & 7;
            u8 fy = py & 7;

            u8 tile = memory_read_8(ppu->p_mem, map + ty * 32 + tx);

            u16 addr =
                (lcdc & (1 << 4))
                ? 0x8000 + tile * 16
                : 0x9000 + ((s8)tile) * 16;

            addr += fy * 2;

            u8 lo = memory_read_8(ppu->p_mem, addr);
            u8 hi = memory_read_8(ppu->p_mem, addr + 1);

            u8 bit = 7 - fx;
            color = ((hi >> bit) & 1) << 1 | ((lo >> bit) & 1);
        }

        bg_line[x] = color;
        ppu->frame_buffer[ppu->ly][x] = bg_palette(ppu, color);
    }

    if (window_active)
        ppu->window_line++;
}


static void render_sprites(PPU *ppu, u8 bg_line[160]) {
    u8 lcdc = memory_read_8(ppu->p_mem, LCDC);
    if (!(lcdc & (1 << 1))) return;

    int sprite_h = (lcdc & (1 << 2)) ? 16 : 8;
    int drawn = 0;

    for (int i = 0; i < 40 && drawn < 10; i++) {
        u16 o = OAM_BASE + i * 4;

        int sy = memory_read_8(ppu->p_mem, o) - 16;
        int sx = memory_read_8(ppu->p_mem, o + 1) - 8;
        u8 tile = memory_read_8(ppu->p_mem, o + 2);
        u8 attr = memory_read_8(ppu->p_mem, o + 3);

        if (ppu->ly < sy || ppu->ly >= sy + sprite_h) continue;
        drawn++;

        bool flip_x = attr & (1 << 5);
        bool flip_y = attr & (1 << 6);
        bool pal1   = attr & (1 << 4);
        bool behind = attr & (1 << 7);

        int y = ppu->ly - sy;
        if (flip_y) y = sprite_h - 1 - y;
        if (sprite_h == 16) tile &= 0xFE;

        u16 addr = 0x8000 + tile * 16 + y * 2;
        u8 lo = memory_read_8(ppu->p_mem, addr);
        u8 hi = memory_read_8(ppu->p_mem, addr + 1);

        for (int px = 0; px < 8; px++) {
            int x = sx + px;
            if (x < 0 || x >= 160) continue;

            int bit = flip_x ? px : (7 - px);
            u8 c = ((hi >> bit) & 1) << 1 | ((lo >> bit) & 1);
            if (c == 0) continue;

            if (behind && bg_line[x] != 0) continue;

            ppu->frame_buffer[ppu->ly][x] =
                obj_palette(ppu, c, pal1);
        }
    }
}


static void render_scanline(PPU *ppu) {
    u8 bg_line[160] = {0};

    render_bg_window(ppu, bg_line);
    render_sprites(ppu, bg_line);
}


static void render_bg_scanline(PPU *ppu) {
    u8 lcdc = get_lcdc(ppu);

    if (!(lcdc & 0x01)) {
        for (int x = 0; x < SCREEN_WIDTH; x++)
            ppu->frame_buffer[ppu->ly][x] = bg_palette_lookup(ppu, 0);
        return;
    }

    u8 scx = ppu->latched_scx;
    u8 scy = ppu->latched_scy;

    bool win_enable = lcdc & (1 << 5);
    u8 wy = memory_read_8(ppu->p_mem, WY);
    u8 wx = memory_read_8(ppu->p_mem, WX);
    int win_x = (int)wx - 7;

    bool window_used_this_line = false;

    for (int x = 0; x < SCREEN_WIDTH; x++) {

        bool use_window =
            win_enable &&
            ppu->ly >= wy &&
            x >= win_x;

        u16 tile_map;
        u8 px, py;

        if (use_window) {
            tile_map = (lcdc & (1 << 6)) ? TILE_MAP_1 : TILE_MAP_0;
            px = x - win_x;
            py = ppu->window_line;
            window_used_this_line = true;
        } else {
            tile_map = (lcdc & 0x08) ? TILE_MAP_1 : TILE_MAP_0;
            px = x + scx;
            py = ppu->ly + scy;
        }

        u8 tile_x = px >> 3;
        u8 tile_y = py >> 3;
        u8 bit_x  = px & 7;
        u8 bit_y  = py & 7;

        u16 map_addr = tile_map + tile_y * 32 + tile_x;
        u8 tile_id = memory_read_8(ppu->p_mem, map_addr);

        u16 tile_addr;
        if (lcdc & 0x10)
            tile_addr = 0x8000 + tile_id * 16;
        else
            tile_addr = 0x9000 + ((s8)tile_id) * 16;

        tile_addr += bit_y * 2;

        u8 lo = memory_read_8(ppu->p_mem, tile_addr);
        u8 hi = memory_read_8(ppu->p_mem, tile_addr + 1);

        u8 bit = 7 - bit_x;
        u8 color = ((hi >> bit) & 1) << 1 | ((lo >> bit) & 1);

        ppu->frame_buffer[ppu->ly][x] =
            bg_palette_lookup(ppu, color);
    }

    if (window_used_this_line)
        ppu->window_line++;
}

/* =========================
   PPU STEP
   ========================= */

void step_ppu(PPU *ppu, int cycles) {
    u8 lcdc = memory_read_8(ppu->p_mem, LCDC);

    if (!(lcdc & 0x80)) {
        ppu->mode = 0;
        ppu->ly = 0;
        ppu->window_line = 0;
        memory_write(ppu->p_mem, LY, 0);
        return;
    }

    ppu->m_cycles += cycles;

    switch (ppu->mode) {

    case 2:
        if (ppu->m_cycles >= 20) {
            ppu->m_cycles -= 20;
            ppu->latched_scx = memory_read_8(ppu->p_mem, SCX);
            ppu->latched_scy = memory_read_8(ppu->p_mem, SCY);
            ppu->mode = 3;
            stat_update(ppu);
            stat_check(ppu);
        }
        break;

    case 3:
        if (ppu->m_cycles >= 43) {
            ppu->m_cycles -= 43;
            render_scanline(ppu);
            ppu->mode = 0;
            stat_update(ppu);
            stat_check(ppu);
        }
        break;

    case 0:
        if (ppu->m_cycles >= 51) {
            ppu->m_cycles -= 51;
            ppu->ly++;
            memory_write(ppu->p_mem, LY, ppu->ly);

            if (ppu->ly == 144) {
                ppu->mode = 1;
                request_interrupt(ppu->ih, VBlank);
                present_framebuffer(ppu->draw_ctx, ppu->frame_buffer);
            } else {
                ppu->mode = 2;
            }
            stat_update(ppu);
            stat_check(ppu);
        }
        break;

    case 1:
        if (ppu->m_cycles >= 114) {
            ppu->m_cycles -= 114;
            ppu->ly++;
            memory_write(ppu->p_mem, LY, ppu->ly);

            if (ppu->ly >= 154) {
                ppu->ly = 0;
                ppu->window_line = 0;
                ppu->mode = 2;
            }
            stat_update(ppu);
            stat_check(ppu);
        }
        break;
    }
}
