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



static inline u8 bg_palette(PPU *ppu, u8 c) {
    u8 p = memory_read_8(ppu->p_mem, BGP);
    return (p >> (c * 2)) & 3;
}

static inline u8 obj_palette(PPU *ppu, u8 c, bool pal1) {
    u8 p = memory_read_8(ppu->p_mem, pal1 ? OBP1 : OBP0);
    return (p >> (c * 2)) & 3;
}

static inline u8 get_lcdc(PPU *ppu) {
    return memory_read_8(ppu->p_mem, LCDC);
}



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

    if (ppu->mode == 0 && (stat & (1 << 3))) req = true;  // HBlank
    if (ppu->mode == 1 && (stat & (1 << 4))) req = true;  // VBlank
    if (ppu->mode == 2 && (stat & (1 << 5))) req = true;  // OAM
    if ((stat & (1 << 6)) && (stat & (1 << 2))) req = true;  // LYC=LY

    // rising edge is a bitch
    if (req && !ppu->stat_irq_line)
        request_interrupt(ppu->ih, LCD);

    ppu->stat_irq_line = req;
}



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
    
    typedef struct {
        int idx;
        int x;
    } SpriteInfo;
    
    SpriteInfo sprites[10];
    int sprite_count = 0;

    for (int i = 0; i < 40 && sprite_count < 10; i++) {
        u16 o = OAM_BASE + i * 4;
        int sy = memory_read_8(ppu->p_mem, o) - 16;
        int sx = memory_read_8(ppu->p_mem, o + 1) - 8;

        if (ppu->ly >= sy && ppu->ly < sy + sprite_h) {
            sprites[sprite_count].idx = i;
            sprites[sprite_count].x = sx;
            sprite_count++;
        }
    }

    for (int i = 0; i < sprite_count - 1; i++) {
        for (int j = i + 1; j < sprite_count; j++) {
            bool swap = false;
            if (sprites[i].x > sprites[j].x) {
                swap = true;
            } else if (sprites[i].x == sprites[j].x && sprites[i].idx > sprites[j].idx) {
                swap = true;
            }
            
            if (swap) {
                SpriteInfo temp = sprites[i];
                sprites[i] = sprites[j];
                sprites[j] = temp;
            }
        }
    }

    for (int s = sprite_count - 1; s >= 0; s--) {
        int i = sprites[s].idx;
        u16 o = OAM_BASE + i * 4;

        int sy = memory_read_8(ppu->p_mem, o) - 16;
        int sx = memory_read_8(ppu->p_mem, o + 1) - 8;
        u8 tile = memory_read_8(ppu->p_mem, o + 2);
        u8 attr = memory_read_8(ppu->p_mem, o + 3);

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

            ppu->frame_buffer[ppu->ly][x] = obj_palette(ppu, c, pal1);
        }
    }
}

static void render_scanline(PPU *ppu) {
    u8 bg_line[160] = {0};

    render_bg_window(ppu, bg_line);
    render_sprites(ppu, bg_line);
}



void step_ppu(PPU *ppu, int cycles) {
    u8 lcdc = memory_read_8(ppu->p_mem, LCDC);

    if (!(lcdc & 0x80)) {
        ppu->mode = 0;
        ppu->ly = 0;
        ppu->window_line = 0;
        ppu->m_cycles = 0;
        memory_write(ppu->p_mem, LY, 0);
        stat_update(ppu);
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
                stat_update(ppu);
                stat_check(ppu);
                request_interrupt(ppu->ih, VBlank);
                present_framebuffer(ppu->draw_ctx, ppu->frame_buffer);
                screen_event_loop(ppu->draw_ctx);
            } else {
                ppu->mode = 2;
                stat_update(ppu);
                stat_check(ppu);
            }
        }
        break;

    case 1:  
        if (ppu->m_cycles >= 114) {
            ppu->m_cycles -= 114;
            ppu->ly++;
            memory_write(ppu->p_mem, LY, ppu->ly);
            stat_update(ppu);
            stat_check(ppu);

            if (ppu->ly >= 154) {
                ppu->ly = 0;
                ppu->window_line = 0;
                memory_write(ppu->p_mem, LY, 0);
                ppu->mode = 2;
                stat_update(ppu);
                stat_check(ppu);
            }
        }
        break;
    }
}