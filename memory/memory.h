/* 
    Memory Contents
*/

#pragma once

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
    u8 WRAM[0x1000];
    u8 IO[0x80];
    u8 IE;
    u8 HRAM[0x7F];
    u8 VRAM[0x2000];
}Memory;

static inline u8 *get_address(Memory *p_mem, const u16 addr){
    if (addr > 0x0000 && addr < 0x8000){
        // Cartridge Rom
        printf("DATA FROM CARTRIDGE ROM AT: %.4xH\n",addr);
        return &p_mem->p_cartidge->rom[addr];
    }

    if (addr >= 0x8000 && addr <=0x9FFF){
        // not implemented vram
        return &p_mem -> VRAM[addr - 0x8000];
    }

    else if (addr >= 0xC000 && addr <= 0xDFFF){
        // WRAM 
        printf("DATA FROM RAM AT: %x\n",addr);
        return &p_mem -> WRAM[addr - 0xC000];
    }

    else if (addr >= 0xFF80 && addr <= 0xFFFE){
        // high ram area
        return &p_mem -> HRAM [addr - 0xFF80];
    }

    else if (addr== 0xff07){
        // some timer shit 
        printf("TIMER STATE IS NOT IMPLEMENTED YET");
        return &p_mem -> IO[0xff07 - 0xff00];
    }
    else if (addr== 0xff0f){
        // some interrupt shit 
        printf("Interrupt  IS NOT IMPLEMENTED YET");
        return &p_mem -> IO[0xff0f - 0xff00];
    }
    else if (addr== 0xffff){
        // more interrupt shit interrupt shit 
        printf("Interrupt  IS NOT IMPLEMENTED YET");
        return &p_mem -> IE;
    }
    else if (addr== 0xff26){
        // audio shit 
        printf("Audio  IS NOT IMPLEMENTED YET");
        return &p_mem -> IO[0xff26 - 0xff00];
    }
    else if (addr== 0xff25){
        // audio sound panning shit 
        printf("Audio  IS NOT IMPLEMENTED YET");
        return &p_mem -> IO[0xff25 - 0xff00];
    }
    else if (addr== 0xff24){
        // audio master volume shit 
        printf("Audio  IS NOT IMPLEMENTED YET");
        return &p_mem -> IO[0xff24 - 0xff00];
    }

    else if (addr == 0xff40){
        // not implemented LCDC : LCD CONTROL
        return &p_mem -> IO[0xff40 - 0xff00];
    }

    else{
        printf("NOT IMPLEMENTED\n");
        return NULL;
    }

}

static inline u8  memory_read_8(Memory *p_mem, const u16 addr){
    printf("READING ");
    if (addr == 0xFF44){
        return 0x90; // TODO: change this just for experiment
    }
    u8 *add =  get_address(p_mem,addr);
    if (add == NULL){
        printf("Reading Unimplemented memory location %x\n",addr);
        exit(0);
    }
    return *add;
}



static inline void memory_write(Memory *p_mem, const u16 addr, const u8 data){
    printf("WRITING ");
    u8 *add =  get_address(p_mem,addr);

    if (add == NULL){
        printf("Writing Unimplemented memory location %x and data %x\n",addr, data);
        exit(0);
    }
    printf("WRITING  AT: %x and Value: %x\n",addr,data);
    *add = data;
   
}


