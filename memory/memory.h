/* 
    Memory Contents
*/

#pragma once

#include <stdbool.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <error.h>
#include "../platform/platform.h"


typedef uint8_t u8;
typedef uint16_t  u16;

typedef struct
{
    Cartridge *p_cartidge;
    Jpad *ctx;

    u8 WRAM[0x2000];
    u8 IO[0x80];
    u8 HRAM[0x7F];
    u8 VRAM[0x2000];
    u8 OAM[0xA0];
    u8 NU[0xFEFF-0xFEA0];
    u8 IE;
    u8 ERAM[0x2000];

    bool is_div_reset;
    u8 stat_shadow;
}Memory;


static inline u8 *get_address(Memory *p_mem, const u16 addr, const bool is_writing){
    if (addr<= 0x7FFF){
        // Cartridge Rom
        #ifdef DEBUG
            printf(" FROM CARTRIDGE ROM AT: %.4xH\n]",addr);
        #endif
        return &p_mem->p_cartidge->rom[addr];
    }

    //DMA
    if (addr == 0xFF46 && is_writing == true){
        printf("DMA\n");
        exit(0);
    }

    if (addr == 0xFF04 && is_writing == true){
        // writing to div resets it to 0
        printf("DIV CAME\n");
        exit(0);
    }

    if (addr >= 0x8000 && addr <=0x9FFF){
        // VRAM
        return &p_mem -> VRAM[addr - 0x8000];
    }

    if (addr >=0xa000 && addr <=0xbfff){
        // external ram
        return &p_mem -> ERAM[addr - 0xa000];
    }

    else if (addr >= 0xC000 && addr <= 0xDFFF){
        // WRAM 
        #ifdef DEBUG
            printf(" FROM WRAM AT: %x]\n",addr);
        #endif
        return &p_mem -> WRAM[addr - 0xC000];
    }

    else if (addr >= 0xFF80 && addr <= 0xFFFE){
        // high ram area
        return &p_mem -> HRAM [addr - 0xFF80];
    }

    else if (addr>= 0xff04 && addr <= 0xff07){
        // some timer shit 
        return &p_mem -> IO[addr - 0xFF00];
    }
    else if (addr== 0xff0f){
        // Interrupt Flag 
        return &p_mem -> IO[addr - 0xFF00];
    }
    else if(addr >=0xE000 && addr <= 0xFDFF){
        return &p_mem -> WRAM[addr - 0xE000];
    }
    else if (addr== 0xffff){
        // Interrupt Enable
        return &p_mem -> IE;
    }
    else if (addr== 0xff26){
        // audio shit 
        return &p_mem -> IO[addr - 0xFF00];
    }
    else if (addr== 0xff25){
        // audio sound panning shit 
        return &p_mem -> IO[addr - 0xFF00];
    }
    else if (addr== 0xff24){
        // audio master volume shit 
        return &p_mem -> IO[addr - 0xFF00];
    }

    else if (addr == 0xff40){
     
        return &p_mem -> IO[addr - 0xFF00];
    }
    else if (addr == 0xff41){
     
        return &p_mem -> IO[addr - 0xFF00];
    }
    else if (addr == 0xff4b || addr == 0xff4a || addr == 0xff06 || addr == 0xff48 || addr == 0xff49 || addr == 0xff44){
        // idk what this is 
     
        return &p_mem -> IO[addr - 0xFF00];
    }
    else if(addr >=0xFF00 && addr<=0xFF7F){
        return &p_mem -> IO[addr -0xFF00];
    }


    else if (addr == 0xFF42 || addr == 0xFF43){
        // scrolling not implemented
        return &p_mem -> IO[addr - 0xFF00];
    }
    else if (addr == 0xFF47){
                // pallete not implemented
        return &p_mem -> IO[addr - 0xFF00];
    }
    else if (addr == 0xFF01 || addr == 0xFF02){
        // serial transfer not implemented
        return &p_mem -> IO [addr - 0xFF00];
    }
    else if (addr >=0xFE00 && addr <=0xFE9F){
        // oam
        return &p_mem ->OAM[addr - 0xFE00];
    }
    else if (addr >=0xFEA0 && addr <=0xFEFF){
        // not usable
        return &p_mem -> NU[addr-0xFEA0];
    }

    else{
        printf("NOT IMPLEMENTED MEMORY LOCATION %x\n",addr);
        exit(1);
        return NULL;
    }

}

static inline u8  memory_read_8(Memory *p_mem, const u16 addr){
    #ifdef DEBUG
        printf("READING ");
    #endif

    if (addr == 0xFF00) {
    u8 val = p_mem->IO[0];   // whatever was written
    u8 res = 0xC0 | (val & 0x30) | 0x0F;

    // buttons
    if (!(val & 0b00100000)) {
        if (p_mem->ctx->a)      res &= ~(1 << 0);
        if (p_mem->ctx->b)      res &= ~(1 << 1);
        if (p_mem->ctx->select) res &= ~(1 << 2);
        if (p_mem->ctx->start)  res &= ~(1 << 3);
    }

    // d pad
    if (!(val & 0b00010000)) {
        if (p_mem->ctx->right) res &= ~(1 << 0);
        if (p_mem->ctx->left)  res &= ~(1 << 1);
        if (p_mem->ctx->up)    res &= ~(1 << 2);
        if (p_mem->ctx->down)  res &= ~(1 << 3);
    }

    return res;
}

    else if(addr == 0xFF41){
        // FF41 is owned by both cpu and ppu
        return p_mem->stat_shadow;
    }

    if (addr == 0xFF44){
        #ifdef LOG
            return 0x90; // TODO: change this just for experiment
        #endif
    }
   
    u8 *add =  get_address(p_mem,addr,false);
    if (add == NULL){
        printf("Reading Unimplemented memory location %x\n",addr);
        exit(0);
    }
    return *add;
}


static inline void dma_start(Memory *mem, u8 value) {
    u16 src = value << 8;

    for (int i = 0; i < 160; i++) {
        *get_address(mem,0xFE00 + i,true) = *get_address(mem,src+i,false);
    }
}


static inline void memory_write(Memory *p_mem, const u16 addr, const u8 data){
    if (addr == 0xFF41){
         u8 old = p_mem->stat_shadow;
        u8 masked = (old & 0x07) | (data & 0x78);
        p_mem->stat_shadow = masked;
        return;
    }
    //DMA
    if (addr == 0xFF46){
       dma_start(p_mem,data);
       return;
    }

    if (addr == 0xFF04){
        // writing to div resets it to 0
        p_mem->is_div_reset = true;
        return ;
    }


    u8 *add =  get_address(p_mem,addr,true);
    
    if (add == NULL){
        printf("Writing Unimplemented memory location %x and data %x\n",addr, data);
        exit(0);
    }
    #ifdef DEBUG
        printf("WRITING  AT: %x and Value: %x\n",addr,data);
    #endif
    *add = data;  
}


